/**
 * @file ota_upload_server.cpp
 * @brief Implements the temporary HTTP firmware upload server.
 */

#include "ota_upload_server.h"

#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <string.h>

namespace {

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

}  // namespace

OtaUploadServer::OtaUploadServer()
    : server_(kHttpPort),
      firmwareName_("unknown"),
      firmwareVersion_("unknown"),
      state_(OtaUploadState::kDisabled),
      initialized_(false),
      serverRunning_(false),
      uploadStarted_(false),
      uploadFinished_(false),
      rebootPending_(false),
      updateWriteError_(false),
      enabledAtMs_(0),
      expectedSize_(0),
      writtenSize_(0),
      lastMessage_{} {}

void OtaUploadServer::begin(const char* firmwareName,
                            const char* firmwareVersion) {
  firmwareName_ = firmwareName != nullptr ? firmwareName : "unknown";
  firmwareVersion_ = firmwareVersion != nullptr ? firmwareVersion : "unknown";

  if (!initialized_) {
    configureRoutes();
    initialized_ = true;
  }

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
    begin(firmwareName_, firmwareVersion_);
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

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* updated = esp_ota_get_next_update_partition(running);
  esp_app_desc_t appDescription = {};
  if (updated != nullptr &&
      esp_ota_get_partition_description(updated, &appDescription) == ESP_OK) {
    snprintf(lastMessage_,
             sizeof(lastMessage_),
             "OTA ready: %s %s",
             appDescription.project_name,
             appDescription.version);
  } else {
    copyMessage(lastMessage_, sizeof(lastMessage_), "OTA ready; rebooting.");
  }

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

    const size_t contentLength = server_.clientContentLength();
    const esp_partition_t* updatePartition =
        esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr) {
      failUpload("OTA upload rejected; no OTA partition available.");
      return;
    }

    if (contentLength == 0 || contentLength > updatePartition->size) {
      failUpload("OTA upload rejected; image size does not fit OTA slot.");
      return;
    }

    if (!Update.begin(contentLength, U_FLASH)) {
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
    expectedSize_ = contentLength;
    writtenSize_ = 0;
    updateWriteError_ = false;
    copyMessage(lastMessage_, sizeof(lastMessage_), "OTA upload started.");
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (state_ != OtaUploadState::kUploading || updateWriteError_) {
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    writtenSize_ += written;
    if (written != upload.currentSize) {
      failUpload("OTA write failed.");
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
