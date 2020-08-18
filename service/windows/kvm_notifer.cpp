// Copyright (c) 2017-2018 Alibaba Group Holding Limited



#include <string>
#include <thread>
#include <windows.h>
#include <tchar.h>
#include "kvm_notifer.h"
#include "utils/Log.h"

struct Error {
	Error() {
		m_class = "GenericError";
		m_decs = "";
	}

	void  setClass(string Class) {
		m_class = Class;
	}

	void   setDesc(string Desc) {
		m_decs = Desc;
	}

	json11::Json  toJson() {
		return json11::Json::object{ {
				"error",
				json11::Json::object{
					{ "class", m_class },
					{ "desc",  m_decs }
				}
			} };
	}
private:
	string  m_class;
	string  m_decs;
};

bool KvmNotifer::is_stopped() 
{
  MutexLocker(&m_mutex)
  {
    return  m_stop;
  }
}

void KvmNotifer::set_stop()
{
  MutexLocker(&m_mutex)
  {
    m_stop = true ;
  }
}


KvmNotifer::KvmNotifer() {
	m_hFile  = INVALID_HANDLE_VALUE;
	m_worker = nullptr;
	m_stop = true;
};


bool KvmNotifer::init(function<void(const char*)> callback) {
	
	m_hFile = CreateFile(_T("\\\\.\\Global\\org.qemu.guest_agent.0"),
		GENERIC_ALL,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	Log::Info("\\\\.\\Global\\org.qemu.guest_agent.0:%d", m_hFile);
	if ( m_hFile == INVALID_HANDLE_VALUE ) {
		return false;
	}

	m_callback = callback;
	m_stop     = false;
	m_worker   = new thread([this]() {
		poll();
	});

	return true;
}

void KvmNotifer::unit() {
  set_stop();
  if (m_worker) {
    m_worker->join();
  }

  if ( m_hFile != INVALID_HANDLE_VALUE ) {
    CloseHandle(m_hFile);
    m_hFile = NULL;
  }
}




bool  KvmNotifer::poll() {

	while ( !is_stopped() ) {
		char  buffer[0x1000] = { 0 };
		DWORD len = 0;
		BOOL  ret = FALSE;

		ret = ReadFile(m_hFile, buffer, sizeof(buffer) - 1, &len, 0);
		if (!ret || len == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
			continue;
		}
		buffer[len] = 0;

#ifdef _DEBUG
		printf("[r]:%s\n", buffer);
#endif

		string output;
		parse(buffer, output);
		Log::Info("[w]:%s\n", output.c_str());
		WriteFile(m_hFile, output.c_str(), output.length(), &len, 0);

#ifdef _DEBUG
		printf("[w]:%s\n", output.c_str());
#endif
	}
    return true;
}

void  KvmNotifer::parse(string input, string& output) {
  Log::Info("command:%s", input.c_str());
  string errinfo;
  auto json = json11::Json::parse(input, errinfo);
  if ( errinfo != "" ) {
    return;
  }

  if (json["execute"] == "guest-sync") {
      return onGuestSync(json["arguments"], output);
  }

  if (json["execute"] == "guest-command") {
      return onGuestCommand(json["arguments"], output);
  }

  if (json["execute"] == "guest-shutdown") {
    return onGuestShutdown(json["arguments"], output);
  }

  Error err;
  err.setDesc("not suport");
  output = err.toJson().dump() + "\n";
}

// gshell check ready
/*{ 'command': 'guest-sync',
'data' : { 'id': 'int' },
'returns' : 'int' }*/
void KvmNotifer::onGuestSync( json11::Json  arguments, string& output ) {
    json11::Json resp = json11::Json::object{ { "return", arguments["id"] } };
    output = resp.dump() + "\n";
}

/*
{ 'command': 'guest-command',
'data': { 'cmd': 'str', 'timeout': 'int' },
'returns': 'GuestCommandResult' }

{ 'type': 'GuestCommandResult',
'data': { 'result': 'int', 'cmd_output': 'str' } }
*/

void  KvmNotifer::onGuestCommand(json11::Json  arguments, string& output) {
  string cmd = arguments["cmd"].string_value();
  if (arguments["cmd"] == "kick_vm" ) {
	Log::Info("receive task notify");
	m_callback("kick_vm");
    json11::Json  GuestCommandResult = json11::Json::object{
        { "result",8 },
        { "cmd_output", "execute kick_vm success" }
    };
    json11::Json  resp = json11::Json::object{ { "return",
        GuestCommandResult } };
    output = resp.dump() + "\n";
  } else {
    Error err;
    err.setDesc("not suport");
    output = err.toJson().dump() + "\n";
  }
}

void  KvmNotifer::onGuestShutdown(json11::Json arguments, string& output) {
  Error err;
  BOOL  bRebootAfterShutdown;

  if ( arguments["mode"].is_null() ) {
    err.setDesc("powerdown|reboot");
    output = err.toJson().dump() + "\n";
    return;
  }

  if (arguments["mode"].string_value() == "powerdown") {
    bRebootAfterShutdown = false;
  } else if (arguments["mode"].string_value() == "reboot") {
    bRebootAfterShutdown = true;
  } else {
    err.setDesc("powerdown|reboot");
    output = err.toJson().dump() + "\n";
    return;
  }

  if (bRebootAfterShutdown) {
	  m_callback("reboot");
  }
  else{
	  m_callback("shutdown");
  }

  json11::Json  GuestCommandResult = json11::Json::object{
	{ "result", 8},
	{ "cmd_output", "execute command success"}
  };


  json11::Json resp = json11::Json::object{ { "return",
	GuestCommandResult } };

  output = resp.dump() + "\n";
}

