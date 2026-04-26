#pragma once

/**
 * @file ota_upload_server.h
 * @brief Temporary HTTP firmware upload endpoint for OTA updates.
 */

#include <Arduino.h>
#include <WebServer.h>

/**
 * @brief Runtime state of the temporary OTA upload server.
 */
enum class OtaUploadState {
  kDisabled,  ///< OTA upload is disabled and the HTTP server is stopped.
  kArmed,     ///< Upload window is open and waiting for a browser upload.
  kUploading, ///< Firmware upload is in progress.
  kSucceeded, ///< Firmware upload finished successfully.
  kFailed,    ///< Firmware upload failed.
  kTimedOut,  ///< Upload window elapsed before completion.
};

/**
 * @brief Hosts a short-lived HTTP form for uploading firmware binaries.
 */
class OtaUploadServer {
 public:
  /**
   * @brief Creates a disabled OTA upload server.
   */
  OtaUploadServer();

  /**
   * @brief Configures HTTP routes and firmware identity strings.
   *
   * @param firmwareName Firmware product name shown in upload pages.
   * @param firmwareVersion Firmware version shown in upload pages.
   */
  void begin(const char* firmwareName,
             const char* firmwareVersion,
             const char* firmwareIdentityTag,
             const char* firmwareVersionTag);

  /**
   * @brief Handles HTTP clients and closes timed-out upload windows.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Opens the temporary OTA upload window.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @param out Stream used for status messages.
   * @return True when the upload window is active.
   */
  bool enable(uint32_t nowMs, Stream& out);

  /**
   * @brief Cancels any active OTA upload window.
   *
   * @param out Stream used for status messages.
   */
  void cancel(Stream& out);

  /**
   * @brief Prints OTA upload status to a stream.
   *
   * @param out Destination stream.
   */
  void printStatus(Stream& out) const;

  /**
   * @brief Indicates whether the upload server is accepting or handling uploads.
   *
   * @return True when the server is armed or uploading.
   */
  bool active() const;

  /**
   * @brief Returns the current OTA upload state.
   *
   * @return Current upload state.
   */
  OtaUploadState state() const;

 private:
  static constexpr uint32_t kUploadWindowMs = 5UL * 60UL * 1000UL;
  static constexpr uint16_t kHttpPort = 80;
  static constexpr size_t kMessageBufferSize = 96;
  static constexpr size_t kIdentityBufferSize = 32;

  void configureRoutes();
  void stopServer(OtaUploadState finalState, const char* message);
  void failUpload(const char* message);
  bool validateUploadedImage();
  void handleRoot();
  void handleUpdateForm();
  void handleUpdateFinished();
  void handleUploadChunk();
  const char* stateLabel() const;

  WebServer server_;
  const char* firmwareName_;
  const char* firmwareVersion_;
  const char* firmwareIdentityTag_;
  const char* firmwareVersionTag_;
  OtaUploadState state_;
  bool initialized_;
  bool serverRunning_;
  bool uploadStarted_;
  bool uploadFinished_;
  bool rebootPending_;
  bool updateWriteError_;
  uint32_t enabledAtMs_;
  size_t maxImageSize_;
  size_t expectedSize_;
  size_t writtenSize_;
  char currentVersion_[kIdentityBufferSize];
  char lastMessage_[kMessageBufferSize];
};
