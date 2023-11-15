// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <cstring>

#include <chiaki/base64.h>

#include "host.h"
#include "io.h"

static void InitAudioCB(unsigned int channels, unsigned int rate, void *user)
{
	IO *io = (IO *)user;
	io->InitAudioCB(channels, rate);
}

static bool VideoCB(uint8_t *buf, size_t buf_size, void *user)
{
	IO *io = (IO *)user;
	return io->VideoCB(buf, buf_size);
}

static void AudioCB(int16_t *buf, size_t samples_count, void *user)
{
	IO *io = (IO *)user;
	io->AudioCB(buf, samples_count);
}

static void EventCB(ChiakiEvent *event, void *user)
{
	Host *host = (Host *)user;
	host->ConnectionEventCB(event);
}

static void RegistEventCB(ChiakiRegistEvent *event, void *user)
{
	Host *host = (Host *)user;
	host->RegistCB(event);
}

Host::Host(std::string host_name)
	: host_name(host_name)
{
	this->settings = Settings::GetInstance();
	this->log = settings->GetLogger();
}

Host::~Host()
{
}

int Host::Wakeup()
{
	if(strlen(this->rp_regist_key) > 8)
	{
		CHIAKI_LOGE(this->log, "Given registkey is too long");
		return 1;
	}
	else if(strlen(this->rp_regist_key) <= 0)
	{
		CHIAKI_LOGE(this->log, "Given registkey is not defined");
		return 2;
	}

	uint64_t credential = (uint64_t)strtoull(this->rp_regist_key, NULL, 16);
	ChiakiErrorCode ret = chiaki_discovery_wakeup(this->log, NULL,
		host_addr.c_str(), credential, this->IsPS5());

	if(ret == CHIAKI_ERR_SUCCESS)
	{
		//FIXME
	}
	return ret;
}

int Host::Register(int pin)
{
	// use pin and accont_id to negociate secrets for session
	//
	std::string account_id = this->settings->GetPSNAccountID(this);
	std::string online_id = this->settings->GetPSNOnlineID(this);
	size_t account_id_size = sizeof(uint8_t[CHIAKI_PSN_ACCOUNT_ID_SIZE]);

	regist_info.target = this->target;

	if(this->target >= CHIAKI_TARGET_PS4_9)
	{
		// use AccountID for ps4 > 7.0
		if(account_id.length() > 0)
		{
			chiaki_base64_decode(account_id.c_str(), account_id.length(),
				regist_info.psn_account_id, &(account_id_size));
			regist_info.psn_online_id = nullptr;
		}
		else
		{
			CHIAKI_LOGE(this->log, "Undefined PSN Account ID (Please configure a valid psn_account_id)");
			return HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID;
		}
	}
	else if(this->target > CHIAKI_TARGET_PS4_UNKNOWN)
	{
		// use oline ID for ps4 < 7.0
		if(online_id.length() > 0)
		{
			regist_info.psn_online_id = this->psn_online_id.c_str();
			// regist_info.psn_account_id = '\0';
		}
		else
		{
			CHIAKI_LOGE(this->log, "Undefined PSN Online ID (Please configure a valid psn_online_id)");
			return HOST_REGISTER_ERROR_SETTING_PSNONLINEID;
		}
	}
	else
	{
		CHIAKI_LOGE(this->log, "Undefined PS4 system version (please run discover first)");
		throw Exception("Undefined PS4 system version (please run discover first)");
	}

	this->regist_info.pin = pin;
	this->regist_info.host = this->host_addr.c_str();
	this->regist_info.broadcast = false;

	if(this->target >= CHIAKI_TARGET_PS4_9)
		CHIAKI_LOGI(this->log, "Registering to host `%s` `%s` with PSN AccountID `%s` pin `%d`",
			this->host_name.c_str(), this->host_addr.c_str(), account_id.c_str(), pin);
	else
		CHIAKI_LOGI(this->log, "Registering to host `%s` `%s` with PSN OnlineID `%s` pin `%d`",
			this->host_name.c_str(), this->host_addr.c_str(), online_id.c_str(), pin);

	chiaki_regist_start(&this->regist, this->log, &this->regist_info, RegistEventCB, this);
	return HOST_REGISTER_OK;
}

int Host::InitSession(IO *user)
{
	chiaki_connect_video_profile_preset(&(this->video_profile),
		this->video_resolution, this->video_fps);
	// Build chiaki ps4 stream session
	chiaki_opus_decoder_init(&(this->opus_decoder), this->log);
	ChiakiAudioSink audio_sink;
	ChiakiConnectInfo chiaki_connect_info = {};

	chiaki_connect_info.host = this->host_addr.c_str();
	chiaki_connect_info.video_profile = this->video_profile;
	chiaki_connect_info.video_profile_auto_downgrade = true;

	chiaki_connect_info.ps5 = this->IsPS5();

	memcpy(chiaki_connect_info.regist_key, this->rp_regist_key, sizeof(chiaki_connect_info.regist_key));
	memcpy(chiaki_connect_info.morning, this->rp_key, sizeof(chiaki_connect_info.morning));

	ChiakiErrorCode err = chiaki_session_init(&(this->session), &chiaki_connect_info, this->log);
	if(err != CHIAKI_ERR_SUCCESS)
		throw Exception(chiaki_error_string(err));
	this->session_init = true;
	// audio setting_cb and frame_cb
	chiaki_opus_decoder_set_cb(&this->opus_decoder, InitAudioCB, AudioCB, user);
	chiaki_opus_decoder_get_sink(&this->opus_decoder, &audio_sink);
	chiaki_session_set_audio_sink(&this->session, &audio_sink);
	chiaki_session_set_video_sample_cb(&this->session, VideoCB, user);
	chiaki_session_set_event_cb(&this->session, EventCB, this);

	// init controller states
	chiaki_controller_state_set_idle(&this->controller_state);

	return 0;
}

int Host::FiniSession()
{
	if(this->session_init)
	{
		this->session_init = false;
		chiaki_session_join(&this->session);
		chiaki_session_fini(&this->session);
		chiaki_opus_decoder_fini(&this->opus_decoder);
	}
	return 0;
}

void Host::StopSession()
{
	chiaki_session_stop(&this->session);
}

void Host::StartSession()
{
	ChiakiErrorCode err = chiaki_session_start(&this->session);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		chiaki_session_fini(&this->session);
		throw Exception("Chiaki Session Start failed");
	}
}

void Host::SendFeedbackState()
{
	// send controller/joystick key
	if(this->io_read_controller_cb != nullptr)
		this->io_read_controller_cb(&this->controller_state, &finger_id_touch_id);

	chiaki_session_set_controller_state(&this->session, &this->controller_state);
}

void Host::ConnectionEventCB(ChiakiEvent *event)
{
	switch(event->type)
	{
		case CHIAKI_EVENT_CONNECTED:
			CHIAKI_LOGI(this->log, "EventCB CHIAKI_EVENT_CONNECTED");
			if(this->chiaki_event_connected_cb != nullptr)
				this->chiaki_event_connected_cb();
			break;
		case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
			CHIAKI_LOGI(this->log, "EventCB CHIAKI_EVENT_LOGIN_PIN_REQUEST");
			if(this->chiaki_even_login_pin_request_cb != nullptr)
				this->chiaki_even_login_pin_request_cb(event->login_pin_request.pin_incorrect);
			break;
		case CHIAKI_EVENT_RUMBLE:
			CHIAKI_LOGD(this->log, "EventCB CHIAKI_EVENT_RUMBLE");
			if(this->chiaki_event_rumble_cb != nullptr)
				this->chiaki_event_rumble_cb(event->rumble.left, event->rumble.right);
			break;
		case CHIAKI_EVENT_QUIT:
			CHIAKI_LOGI(this->log, "EventCB CHIAKI_EVENT_QUIT");
			if(this->chiaki_event_quit_cb != nullptr)
				this->chiaki_event_quit_cb(&event->quit);
			break;
	}
}

void Host::RegistCB(ChiakiRegistEvent *event)
{
	// Chiaki callback fuction
	// fuction called by lib chiaki regist
	// durring client pin code registration
	//
	// read data from lib and record secrets into Host object

	this->registered = false;
	switch(event->type)
	{
		case CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED:
			CHIAKI_LOGI(this->log, "Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED");
			if(this->chiaki_regist_event_type_finished_canceled != nullptr)
			{
				this->chiaki_regist_event_type_finished_canceled();
			}
			break;
		case CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED:
			CHIAKI_LOGI(this->log, "Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED");
			if(this->chiaki_regist_event_type_finished_failed != nullptr)
			{
				this->chiaki_regist_event_type_finished_failed();
			}
			break;
		case CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS:
		{
			ChiakiRegisteredHost *r_host = event->registered_host;
			CHIAKI_LOGI(this->log, "Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS");
			// copy values form ChiakiRegisteredHost object
			this->ap_ssid = r_host->ap_ssid;
			this->ap_key = r_host->ap_key;
			this->ap_name = r_host->ap_name;
			memcpy(&(this->server_mac), &(r_host->server_mac), sizeof(this->server_mac));
			this->server_nickname = r_host->server_nickname;
			memcpy(&(this->rp_regist_key), &(r_host->rp_regist_key), sizeof(this->rp_regist_key));
			this->rp_key_type = r_host->rp_key_type;
			memcpy(&(this->rp_key), &(r_host->rp_key), sizeof(this->rp_key));
			// mark host as registered
			this->registered = true;
			this->rp_key_data = true;
			CHIAKI_LOGI(this->log, "Register Success %s", this->host_name.c_str());

			if(this->chiaki_regist_event_type_finished_success != nullptr)
				this->chiaki_regist_event_type_finished_success();

			break;
		}
	}
	// close registration socket
	chiaki_regist_stop(&this->regist);
	chiaki_regist_fini(&this->regist);
}

bool Host::GetVideoResolution(int *ret_width, int *ret_height)
{
	switch(this->video_resolution)
	{
		case CHIAKI_VIDEO_RESOLUTION_PRESET_360p:
			*ret_width = 640;
			*ret_height = 360;
			break;
		case CHIAKI_VIDEO_RESOLUTION_PRESET_540p:
			*ret_width = 950;
			*ret_height = 540;
			break;
		case CHIAKI_VIDEO_RESOLUTION_PRESET_720p:
			*ret_width = 1280;
			*ret_height = 720;
			break;
		case CHIAKI_VIDEO_RESOLUTION_PRESET_1080p:
			*ret_width = 1920;
			*ret_height = 1080;
			break;
		case CHIAKI_VIDEO_RESOLUTION_PRESET_2160p:
			*ret_width = 3840;
			*ret_height = 2160;
			break;
		default:
			return false;
	}
	return true;
}

std::string Host::GetHostName()
{
	return this->host_name;
}

std::string Host::GetHostAddr()
{
	return this->host_addr;
}

ChiakiTarget Host::GetChiakiTarget()
{
	return this->target;
}

void Host::SetChiakiTarget(ChiakiTarget target)
{
	this->target = target;
}

void Host::SetHostAddr(std::string host_addr)
{
	this->host_addr = host_addr;
}

void Host::SetRegistEventTypeFinishedCanceled(std::function<void()> chiaki_regist_event_type_finished_canceled)
{
	this->chiaki_regist_event_type_finished_canceled = chiaki_regist_event_type_finished_canceled;
}

void Host::SetRegistEventTypeFinishedFailed(std::function<void()> chiaki_regist_event_type_finished_failed)
{
	this->chiaki_regist_event_type_finished_failed = chiaki_regist_event_type_finished_failed;
}

void Host::SetRegistEventTypeFinishedSuccess(std::function<void()> chiaki_regist_event_type_finished_success)
{
	this->chiaki_regist_event_type_finished_success = chiaki_regist_event_type_finished_success;
}

void Host::SetEventConnectedCallback(std::function<void()> chiaki_event_connected_cb)
{
	this->chiaki_event_connected_cb = chiaki_event_connected_cb;
}

void Host::SetEventLoginPinRequestCallback(std::function<void(bool)> chiaki_even_login_pin_request_cb)
{
	this->chiaki_even_login_pin_request_cb = chiaki_even_login_pin_request_cb;
}

void Host::SetEventQuitCallback(std::function<void(ChiakiQuitEvent *)> chiaki_event_quit_cb)
{
	this->chiaki_event_quit_cb = chiaki_event_quit_cb;
}

void Host::SetEventRumbleCallback(std::function<void(uint8_t, uint8_t)> chiaki_event_rumble_cb)
{
	this->chiaki_event_rumble_cb = chiaki_event_rumble_cb;
}

void Host::SetReadControllerCallback(std::function<void(ChiakiControllerState *, std::map<uint32_t, int8_t> *)> io_read_controller_cb)
{
	this->io_read_controller_cb = io_read_controller_cb;
}

bool Host::IsRegistered()
{
	return this->registered;
}

bool Host::IsDiscovered()
{
	return this->discovered;
}

bool Host::IsReady()
{
	return this->state == CHIAKI_DISCOVERY_HOST_STATE_READY;
}

bool Host::HasRPkey()
{
	return this->rp_key_data;
}

bool Host::IsPS5()
{
	if(this->target >= CHIAKI_TARGET_PS5_UNKNOWN)
		return true;
	else
		return false;
}
