#pragma once

#include <cstdint>
#include <atomic>
#include <stdexcept>
#include <functional>
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#ifndef _OPENVR_API
#include <openvr_driver.h>
#endif

#define OPENVR_SPACECALIBRATOR_SOCKET_PATH "/tmp/OpenVRSpaceCalibratorDriver"
#define OPENVR_SPACECALIBRATOR_SHMEM_NAME "/OpenVRSpaceCalibratorPoseMemoryV1"

#ifdef _OPENVR_API 

namespace vr {
	struct DriverPose_t
	{
		double poseTimeOffset;

		vr::HmdQuaternion_t qWorldFromDriverRotation;
		double vecWorldFromDriverTranslation[3];

		vr::HmdQuaternion_t qDriverFromHeadRotation;
		double vecDriverFromHeadTranslation[3];

		double vecPosition[3];
		double vecVelocity[3];
		double vecAcceleration[3];
		vr::HmdQuaternion_t qRotation;
		double vecAngularVelocity[3];
		double vecAngularAcceleration[3];

		ETrackingResult result;

		bool poseIsValid;
		bool willDriftInYaw;
		bool shouldApplyHeadModel;
		bool deviceIsConnected;
	};

}

#endif

namespace protocol
{
	const uint32_t Version = 4;

	enum RequestType
	{
		RequestInvalid,
		RequestHandshake,
		RequestSetDeviceTransform,
		RequestSetAlignmentSpeedParams,
		RequestDebugOffset
	};

	enum ResponseType
	{
		ResponseInvalid,
		ResponseHandshake,
		ResponseSuccess,
	};

	struct Protocol
	{
		uint32_t version = Version;
	};

	struct AlignmentSpeedParams
	{
		double thr_trans_tiny, thr_trans_small, thr_trans_large;
		double thr_rot_tiny, thr_rot_small, thr_rot_large;
		double align_speed_tiny, align_speed_small, align_speed_large;
	};

	struct SetDeviceTransform
	{
		uint32_t openVRID;
		bool enabled;
		bool updateTranslation;
		bool updateRotation;
		bool updateScale;
		vr::HmdVector3d_t translation;
		vr::HmdQuaternion_t rotation;
		double scale;
		bool lerp;
		bool quash;

		SetDeviceTransform(uint32_t id, bool enabled) :
			openVRID(id), enabled(enabled), updateTranslation(false), updateRotation(false), updateScale(false), translation({}), rotation({1,0,0,0}), scale(1), lerp(false), quash(false) { }

		SetDeviceTransform(uint32_t id, bool enabled, vr::HmdVector3d_t translation) :
			openVRID(id), enabled(enabled), updateTranslation(true), updateRotation(false), updateScale(false), translation(translation), rotation({ 1,0,0,0 }), scale(1), lerp(false), quash(false) { }

		SetDeviceTransform(uint32_t id, bool enabled, vr::HmdQuaternion_t rotation) :
			openVRID(id), enabled(enabled), updateTranslation(false), updateRotation(true), updateScale(false), translation({}), rotation(rotation), scale(1), lerp(false), quash(false) { }

		SetDeviceTransform(uint32_t id, bool enabled, double scale) :
			openVRID(id), enabled(enabled), updateTranslation(false), updateRotation(false), updateScale(true), translation({}), rotation({ 1,0,0,0 }), scale(scale), lerp(false), quash(false) { }

		SetDeviceTransform(uint32_t id, bool enabled, vr::HmdVector3d_t translation, vr::HmdQuaternion_t rotation) :
			openVRID(id), enabled(enabled), updateTranslation(true), updateRotation(true), updateScale(false), translation(translation), rotation(rotation), scale(1), lerp(false), quash(false) { }

		SetDeviceTransform(uint32_t id, bool enabled, vr::HmdVector3d_t translation, vr::HmdQuaternion_t rotation, double scale) :
			openVRID(id), enabled(enabled), updateTranslation(true), updateRotation(true), updateScale(true), translation(translation), rotation(rotation), scale(scale), lerp(false), quash(false) { }
	};

	struct Request
	{
		RequestType type;

		union {
			SetDeviceTransform setDeviceTransform;
			AlignmentSpeedParams setAlignmentSpeedParams;
		};

		Request() : type(RequestInvalid), setAlignmentSpeedParams({}) { }
		Request(RequestType type) : type(type), setAlignmentSpeedParams({}) { }
		Request(AlignmentSpeedParams params) : type(RequestType::RequestSetAlignmentSpeedParams), setAlignmentSpeedParams(params) {}
	};

	struct Response
	{
		ResponseType type;

		union {
			Protocol protocol;
		};

		Response() : type(ResponseInvalid), protocol({}) {}
		Response(ResponseType type) : type(type), protocol({}) { }
	};

	class DriverPoseShmem {
	public:
		struct AugmentedPose {
			timespec sample_time;
			int deviceId;
			vr::DriverPose_t pose;
		};
	private:
		static const uint32_t SYNC_ACTIVE_POSE_B = 0x80000000;
		static const uint32_t BUFFERED_SAMPLES = 64 * 1024;

		struct ShmemData {
			std::atomic<uint64_t> index;
			AugmentedPose poses[BUFFERED_SAMPLES];
		};
		
	private:
		int shm_fd;
		ShmemData* pData;
		uint64_t cursor;

		AugmentedPose lastPose[vr::k_unMaxTrackedDeviceCount] = {0};

		std::string LastErrorString(int errnum)
		{
			char buffer[256];
			char* msg = strerror_r(errnum, buffer, sizeof(buffer));
			return std::string(msg);
		}

		timespec GetCurrentTime()
		{
			timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			return ts;
		}

	public:
		operator bool() const {
			return pData != nullptr;
		}

		bool operator!() const {
			return pData == nullptr;
		}

		DriverPoseShmem() {
			shm_fd = -1;
			pData = nullptr;
			cursor = 0;
		}

		~DriverPoseShmem() {
			Close();
		}

		void Close() {
			if (pData) {
				munmap(pData, sizeof(ShmemData));
				pData = nullptr;
			}
			if (shm_fd >= 0) {
				close(shm_fd);
				shm_fd = -1;
			}
		}

		bool Create(const char* segment_name) {
			Close();

			shm_fd = shm_open(segment_name, O_CREAT | O_RDWR, 0666);
			if (shm_fd < 0) {
				return false;
			}

			if (ftruncate(shm_fd, sizeof(ShmemData)) < 0) {
				close(shm_fd);
				shm_fd = -1;
				return false;
			}

			pData = reinterpret_cast<ShmemData*>(mmap(nullptr, sizeof(ShmemData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
			if (pData == MAP_FAILED) {
				close(shm_fd);
				shm_fd = -1;
				pData = nullptr;
				return false;
			}

			new (pData) ShmemData();
			return true;
		}

		void Open(const char* segment_name) {
			Close();

			shm_fd = shm_open(segment_name, O_RDWR, 0);
			if (shm_fd < 0) {
				throw std::runtime_error("Failed to open pose data shared memory segment: " + LastErrorString(errno));
			}

			pData = reinterpret_cast<ShmemData*>(mmap(nullptr, sizeof(ShmemData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
			if (pData == MAP_FAILED) {
				close(shm_fd);
				shm_fd = -1;
				throw std::runtime_error("Failed to map pose data shared memory segment: " + LastErrorString(errno));
			}
		}

		void ReadNewPoses(std::function<void(AugmentedPose const&)> cb) {
			if (!pData) throw std::runtime_error("Not open");
			
			uint64_t cur_index = pData->index.load(std::memory_order_acquire);
			if (cur_index < cursor || cur_index - cursor > BUFFERED_SAMPLES / 2) {
				if (cur_index < BUFFERED_SAMPLES / 2)
					cursor = cur_index;
				else
					cursor = cur_index - BUFFERED_SAMPLES / 2;
			}

			while (cursor < cur_index) {
				cb(pData->poses[cursor % BUFFERED_SAMPLES]);
				cursor++;
			}

			std::atomic_thread_fence(std::memory_order_release);
		}

		bool GetPose(int index, vr::DriverPose_t& pose, timespec *pSampleTime = nullptr) {
			ReadNewPoses([this](AugmentedPose const& pose) {
				if (pose.pose.poseIsValid && pose.pose.result == vr::ETrackingResult::TrackingResult_Running_OK) {
					this->lastPose[pose.deviceId] = pose;
				}
			});

			if (index >= 0 && index < vr::k_unMaxTrackedDeviceCount) {
				pose = lastPose[index].pose;
				if (pSampleTime) *pSampleTime = lastPose[index].sample_time;
				return true;
			}
			return false;
		}

		void SetPose(int index, const vr::DriverPose_t& pose) {
			if (index >= vr::k_unMaxTrackedDeviceCount) return;
			if (pData == nullptr) return;

			AugmentedPose augPose = {0};
			augPose.deviceId = index;
			augPose.pose = pose;
			augPose.sample_time = GetCurrentTime();

			uint64_t cur_index = pData->index.load(std::memory_order_relaxed) + 1;
			pData->poses[cur_index % BUFFERED_SAMPLES] = augPose;
			pData->index.store(cur_index, std::memory_order_release);
		}
	};
}

