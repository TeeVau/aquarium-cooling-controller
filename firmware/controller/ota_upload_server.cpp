/**
 * @file ota_upload_server.cpp
 * @brief Implements the temporary HTTP firmware upload server.
 */

#include "ota_upload_server.h"

#include <ESP.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <string.h>

namespace {

constexpr char kVersionTagPrefix[] = "AQFW_VERSION=";

const char kUploadForm[] =
    "<!doctype html><html><head><meta name='viewport' "
    "content='width=device-width,initial-scale=1'><title>OTA Update</title>"
    "</head><body><h1>Aquarium Controller OTA</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin' required>"
    "<button type='submit'>Upload firmware</button>"
    "</form></body></html>";

void copyMessage(char* target, size_t targetSize, const char* message) {
  if (targetSize == 0) {
    return;
  }

  if (message == nullptr) {
    message = "";
  }

  snprintf(target, targetSize, "%s", message);
}

bool parseSemVerCore(const char* version,
                     uint32_t* major,
                     uint32_t* minor,
                     uint32_t* patch) {
  if (version == nullptr || major == nullptr || minor == nullptr || patch == nullptr) {
    return false;
  }

  char* endPtr = nullptr;
  const unsigned long parsedMajor = strtoul(version, &endPtr, 10);
  if (endPtr == version || *endPtr != '.') {
    return false;
  }

  const char* minorStart = endPtr + 1;
  const unsigned long parsedMinor = strtoul(minorStart, &endPtr, 10);
  if (endPtr == minorStart || *endPtr != '.') {
    return false;
  }

  const char* patchStart = endPtr + 1;
  const unsigned long parsedPatch = strtoul(patchStart, &endPtr, 10);
  if (endPtr == patchStart) {
    return false;
  }

  if (*endPtr != '\0' && *endPtr != '-' && *endPtr != '+') {
    return false;
  }

  *major = (uint32_t)parsedMajor;
  *minor = (uint32_t)parsedMinor;
  *patch = (uint32_t)parsedPatch;
  return true;
}

int compareSemVer(const char* left, const char* right) {
  uint32_t leftMajor = 0;
  uint32_t leftMinor = 0;
  uint32_t leftPatch = 0;
  uint32_t rightMajor = 0;
  uint32_t rightMinor = 0;
  uint32_t rightPatch = 0;

  if (!parseSemVerCore(left, &leftMajor, &leftMinor, &leftPatch) ||
      !parseSemVerCore(right, &rightMajor, &rightMinor, &rightPatch)) {
    return 0;
  }

  if (leftMajor != rightMajor) {
    return leftMajor < rightMajor ? -1 : 1;
  }

  if (leftMinor != rightMinor) {
    return leftMinor < rightMinor ? -1 : 1;
  }

  if (leftPatch != rightPatch) {
    return leftPatch < rightPatch ? -1 : 1;
  }

  return 0;
}

bool partitionContainsTag(const esp_partition_t* partition,
                          size_t imageSize,
                          const char* tag) {
  if (partition == nullptr || tag == nullptr || tag[0] == '\0' || imageSize == 0) {
    return false;
  }

  const size_t tagLength = strlen(tag);
  if (tagLength == 0 || imageSize < tagLength) {
    return false;
  }

  constexpr size_t kChunkSize = 256;
  uint8_t buffer[kChunkSize + 64] = {};
  size_t carry = 0;
  size_t offset = 0;

  while (offset < imageSize) {
    const size_t toRead = (imageSize - offset) < kChunkSize ? (imageSize - offset) : kChunkSize;
    if (esp_partition_read(partition, offset, buffer + carry, toRead) != ESP_OK) {
      return false;
    }

    const size_t windowSize = carry + toRead;
    for (size_t index = 0; index + tagLength <= windowSize; ++index) {
      if (memcmp(buffer + index, tag, tagLength) == 0) {
        return true;
      }
    }

    carry = tagLength > 1 ? tagLength - 1 : 0;
    if (carry > windowSize) {
      carry = windowSize;
    }
    memmove(buffer, buffer + windowSize - carry, carry);
    offset += toRead;
  }

  return false;
}

bool readNullTerminatedValue(const esp_partition_t* partition,
                             size_t imageSize,
                             size_t valueOffset,
                             char* target,
                             size_t targetSize) {
  if (partition == nullptr || target == nullptr || targetSize == 0 || valueOffset >= imageSize) {
    return false;
  }

  size_t written = 0;
  while (valueOffset < imageSize && written + 1 < targetSize) {
    char nextChar = '\0';
    if (esp_partition_read(partition, valueOffset, &nextChar, 1) != ESP_OK) {
      return false;
    }

    if (nextChar == '\0') {
      target[written] = '\0';
      return written > 0;
    }

    target[written++] = nextChar;
    ++valueOffset;
  }

  if (written < targetSize) {
    target[written] = '\0';
  } else {
    target[targetSize - 1] = '\0';
  }

  return false;
}

bool extractTagValueFromPartition(const esp_partition_t* partition,
                                  size_t imageSize,
                                  const char* prefix,
                                  char* target,
                                  size_t targetSize) {
  if (partition == nullptr || prefix == nullptr || target == nullptr || targetSize == 0) {
    return false;
  }

  target[0] = '\0';
  const size_t prefixLength = strlen(prefix);
  if (prefixLength == 0 || imageSize <= prefixLength) {
    return false;
  }

  constexpr size_t kChunkSize = 256;
  uint8_t buffer[kChunkSize + 64] = {};
  size_t carry = 0;
  size_t offset = 0;

  while (offset < imageSize) {
    const size_t toRead = (imageSize - offset) < kChunkSize ? (imageSize - offset) : kChunkSize;
    if (esp_partition_read(partition, offset, buffer + carry, toRead) != ESP_OK) {
      return false;
    }

    const size_t windowSize = carry + toRead;
    for (size_t index = 0; index + prefixLength <= windowSize; ++index) {
      if (memcmp(buffer + index, prefix, prefixLength) == 0) {
        const size_t absoluteOffset = offset - carry + index + prefixLength;
        return readNullTerminatedValue(
            partition, imageSize, absoluteOffset, target, targetSize);
      }
    }

    carry = prefixLength > 1 ? prefixLength - 1 : 0;
    if (carry > windowSize) {
      carry = windowSize;
    }
    memmove(buffer, buffer + windowSize - carry, carry);
    offset += toRead;
  }

  return false;
}

}  // namespace

OtaUploadServer::OtaUploadServer()
    : server_(kHttpPort),
      firmwareName_("unknown"),
      firmwareVersion_("unknown"),
      firmwareIdentityTag_(""),
      firmwareVersionTag_(""),
      state_(OtaUploadState::kDisabled),
      initialized_(false),
      serverRunning_(false),
      uploadStarted_(false),
      uploadFinished_(false),
      rebootPending_(false),
      updateWriteError_(false),
      enabledAtMs_(0),
      maxImageSize_(0),
      expectedSize_(0),
      writtenSize_(0),
      currentVersion_{},
      lastMessage_{} {}

void OtaUploadServer::begin(const char* firmwareName,
                            const char* firmwareVersion,
                            const char* firmwareIdentityTag,
                            const char* firmwareVersionTag) {
  firmwareName_ = firmwareName != nullptr ? firmwareName : "unknown";
  firmwareVersion_ = firmwareVersion != nullptr ? firmwareVersion : "unknown";
  firmwareIdentityTag_ = firmwareIdentityTag != nullptr ? firmwareIdentityTag : "";
  firmwareVersionTag_ = firmwareVersionTag != nullptr ? firmwareVersionTag : "";

  if (!initialized_) {
    configureRoutes();
    initialized_ = true;
  }

  copyMessage(currentVersion_, sizeof(currentVersion_), firmwareVersion_);

  copyMessage(lastMessage_, sizeof(lastMessage_), "OTA upload disabled.");
}

void OtaUploadServer::update(uint32_t nowMs) {
  if (!serverRunning_) {
    return;
  }

  server_.handleClient();

  if (state_ == OtaUploadState::kArmed &&
      nowMs - enabledAtMs_ >= kUploadWindowMs) {
    stopServer(OtaUploadState::kTimedOut, "OTA upload window timed out.");
  }

  if (rebootPending_) {
    delay(250);
    ESP.restart();
  }
}

bool OtaUploadServer::enable(uint32_t nowMs, Stream& out) {
  if (!initialized_) {
    begin(firmwareName_, firmwareVersion_, firmwareIdentityTag_, firmwareVersionTag_);
  }

  if (WiFi.status() != WL_CONNECTED) {
    state_ = OtaUploadState::kFailed;
    copyMessage(lastMessage_,
                sizeof(lastMessage_),
                "Wi-Fi is not connected; OTA upload not enabled.");
    out.println(lastMessage_);
    return false;
  }

  if (serverRunning_) {
    out.println("OTA upload window is already active.");
    printStatus(out);
    return true;
  }

  state_ = OtaUploadState::kArmed;
  enabledAtMs_ = nowMs;
  maxImageSize_ = 0;
  expectedSize_ = 0;
  writtenSize_ = 0;
  uploadStarted_ = false;
  uploadFinished_ = false;
  rebootPending_ = false;
  updateWriteError_ = false;
  copyMessage(lastMessage_,
              sizeof(lastMessage_),
              "OTA upload window active.");

  server_.begin();
  serverRunning_ = true;

  out.println("OTA upload window active for 300 seconds.");
  out.print("Upload URL: http://");
  out.print(WiFi.localIP());
  out.println("/update");
  return true;
}

void OtaUploadServer::cancel(Stream& out) {
  if (state_ == OtaUploadState::kUploading) {
    Update.abort();
    updateWriteError_ = true;
  }

  if (!serverRunning_) {
    state_ = OtaUploadState::kDisabled;
    copyMessage(lastMessage_, sizeof(lastMessage_), "OTA upload disabled.");
    out.println("OTA upload is not active.");
    return;
  }

  stopServer(OtaUploadState::kDisabled, "OTA upload cancelled.");
  out.println(lastMessage_);
}

void OtaUploadServer::printStatus(Stream& out) const {
  out.println("OTA upload:");
  out.print("  State: ");
  out.println(stateLabel());
  out.print("  Firmware: ");
  out.print(firmwareName_);
  out.print(" ");
  out.println(firmwareVersion_);
  out.print("  HTTP server active: ");
  out.println(serverRunning_ ? "yes" : "no");
  out.print("  Upload started: ");
  out.println(uploadStarted_ ? "yes" : "no");
  out.print("  Upload bytes: ");
  out.print(writtenSize_);
  if (expectedSize_ > 0) {
    out.print(" / ");
    out.print(expectedSize_);
  }
  out.println();
  out.print("  Last message: ");
  out.println(lastMessage_);

  if (serverRunning_ && WiFi.status() == WL_CONNECTED) {
    out.print("  Upload URL: http://");
    out.print(WiFi.localIP());
    out.println("/update");
  }
}

bool OtaUploadServer::active() const {
  return serverRunning_;
}

OtaUploadState OtaUploadServer::state() const {
  return state_;
}

void OtaUploadServer::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/update", HTTP_GET, [this]() { handleUpdateForm(); });
  server_.on(
      "/update",
      HTTP_POST,
      [this]() { handleUpdateFinished(); },
      [this]() { handleUploadChunk(); });

  server_.onNotFound([this]() {
    server_.send(404, "text/plain", "Not found. Use /update while OTA is enabled.");
  });
}

void OtaUploadServer::stopServer(OtaUploadState finalState,
                                 const char* message) {
  if (serverRunning_) {
    server_.close();
  }

  serverRunning_ = false;
  state_ = finalState;
  copyMessage(lastMessage_, sizeof(lastMessage_), message);
}

void OtaUploadServer::failUpload(const char* message) {
  Update.abort();
  updateWriteError_ = true;
  state_ = OtaUploadState::kFailed;
  copyMessage(lastMessage_, sizeof(lastMessage_), message);
}

bool OtaUploadServer::validateUploadedImage() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* updated = esp_ota_get_next_update_partition(running);
  if (updated == nullptr) {
    copyMessage(lastMessage_, sizeof(lastMessage_), "OTA validation failed: no OTA partition.");
    return false;
  }

  if (!partitionContainsTag(updated, writtenSize_, firmwareIdentityTag_)) {
    copyMessage(lastMessage_,
                sizeof(lastMessage_),
                "OTA validation failed: wrong firmware identity.");
    return false;
  }

  char uploadedVersion[kIdentityBufferSize] = {};
  if (!extractTagValueFromPartition(
          updated, writtenSize_, kVersionTagPrefix, uploadedVersion, sizeof(uploadedVersion))) {
    copyMessage(lastMessage_,
                sizeof(lastMessage_),
                "OTA validation failed: firmware version tag missing.");
    return false;
  }

  const int versionCompare = compareSemVer(uploadedVersion, currentVersion_);
  if (versionCompare <= 0) {
    snprintf(lastMessage_,
             sizeof(lastMessage_),
              "OTA validation failed: version %s is not newer than %s.",
             uploadedVersion,
             currentVersion_);
    return false;
  }

  snprintf(lastMessage_,
           sizeof(lastMessage_),
           "OTA ready: %s %s",
           firmwareName_,
           uploadedVersion);
  return true;
}

void OtaUploadServer::handleRoot() {
  handleUpdateForm();
}

void OtaUploadServer::handleUpdateForm() {
  if (state_ != OtaUploadState::kArmed) {
    server_.send(503, "text/plain", "OTA upload is not enabled.");
    return;
  }

  server_.send(200, "text/html", kUploadForm);
}

void OtaUploadServer::handleUpdateFinished() {
  if (state_ != OtaUploadState::kUploading || updateWriteError_) {
    server_.send(500, "text/plain", lastMessage_);
    stopServer(OtaUploadState::kFailed, lastMessage_);
    return;
  }

  if (!validateUploadedImage()) {
    failUpload(lastMessage_);
    server_.send(400, "text/plain", lastMessage_);
    stopServer(OtaUploadState::kFailed, lastMessage_);
    return;
  }

  if (expectedSize_ == 0 || writtenSize_ != expectedSize_) {
    failUpload("OTA validation failed: upload incomplete.");
    server_.send(400, "text/plain", lastMessage_);
    stopServer(OtaUploadState::kFailed, lastMessage_);
    return;
  }

  if (!Update.end(true)) {
    char message[kMessageBufferSize] = {};
    snprintf(message,
             sizeof(message),
             "OTA validation failed: %s",
             Update.errorString());
    failUpload(message);
    server_.send(500, "text/plain", lastMessage_);
    stopServer(OtaUploadState::kFailed, lastMessage_);
    return;
  }

  uploadFinished_ = true;

  state_ = OtaUploadState::kSucceeded;
  server_.send(200, "text/plain", "OTA upload successful. Rebooting.");
  serverRunning_ = false;
  server_.close();
  rebootPending_ = true;
}

void OtaUploadServer::handleUploadChunk() {
  HTTPUpload& upload = server_.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (state_ != OtaUploadState::kArmed || uploadStarted_) {
      failUpload("OTA upload rejected; upload window already used.");
      return;
    }

    const esp_partition_t* updatePartition =
        esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr) {
      failUpload("OTA upload rejected; no OTA partition available.");
      return;
    }

    maxImageSize_ = updatePartition->size;
    if (maxImageSize_ == 0) {
      failUpload("OTA upload rejected; image size does not fit OTA slot.");
      return;
    }

    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      char message[kMessageBufferSize] = {};
      snprintf(message,
               sizeof(message),
               "OTA begin failed: %s",
               Update.errorString());
      failUpload(message);
      return;
    }

    state_ = OtaUploadState::kUploading;
    uploadStarted_ = true;
    expectedSize_ = 0;
    writtenSize_ = 0;
    updateWriteError_ = false;
    copyMessage(lastMessage_, sizeof(lastMessage_), "OTA upload started.");
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (state_ != OtaUploadState::kUploading || updateWriteError_) {
      return;
    }

    if (upload.currentSize == 0 || writtenSize_ + upload.currentSize > maxImageSize_) {
      failUpload("OTA upload rejected; image size does not fit OTA slot.");
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    writtenSize_ += written;
    if (written != upload.currentSize) {
      failUpload("OTA write failed.");
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    expectedSize_ = upload.totalSize;
    if (expectedSize_ == 0 || writtenSize_ != expectedSize_) {
      failUpload("OTA validation failed: upload incomplete.");
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    failUpload("OTA upload aborted.");
  }
}

const char* OtaUploadServer::stateLabel() const {
  switch (state_) {
    case OtaUploadState::kDisabled:
      return "disabled";
    case OtaUploadState::kArmed:
      return "armed";
    case OtaUploadState::kUploading:
      return "uploading";
    case OtaUploadState::kSucceeded:
      return "succeeded";
    case OtaUploadState::kFailed:
      return "failed";
    case OtaUploadState::kTimedOut:
      return "timed-out";
    default:
      return "unknown";
  }
}
