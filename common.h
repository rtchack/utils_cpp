/**
 *  Created on: Mar 5, 2016
 *      Author: xing
 */

#pragma once

#include <string>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

#include "macro_utils.h"

#ifndef ENABLE_CPP_EXCEPTION
#include "log.h"
#endif

namespace utils {

/**
 * return value
 */
enum class Ret {
#define UTILS_RET_ERROR 0x8000;

	OK = 0, // OK
	NO, // no

	E_ARG, // parameter error
	E_ARG_NULL, // nil parameter detected

	E_MEM, // memory allocation error
	E_INIT, // not properly initialized yet

	E_FILE_OP, // file operation error
	E_FILE_CON, // file content error

	E_UNSUP, // unsupported type
	E_ALG, // failed on algorithm
	E_BUF_OVERFLOW, // buffer overflow

	E_IO_PENDING, // operation could not be completed synchronously

	E_SOCKET_NOT_CONNECTED, // socket is not currently connected

	E_GENERAL // general error
};

std::string to_string(const Ret value) noexcept;

constexpr bool Failed(const Ret ret) noexcept {
	return ret > Ret::NO;
}

constexpr bool Succeed(const Ret ret) noexcept {
	return !Failed(ret);
}

#ifdef ENABLE_CPP_EXCEPTION

/**
 * exception
 */
class Excep : public std::exception {
 public:
	Excep() = delete;

	Excep(const std::string &message) :
			msg{std::to_string(getpid()) + ": " + message} {}

	Excep(Ret ret) : msg{to_string(ret)} {}

	Excep(Ret ret, const std::string &message) : Excep(ret) {
		msg += ": ";
		msg += message;
	};

	Excep(const Excep &other) noexcept : msg{other.msg} {}

	const char *what() const _GLIBCXX_USE_NOEXCEPT override {
		return msg.c_str();
	};

 private:
	std::string msg;
};


#define UTILS_RAISE(msg) \
	std::stringstream utilscpp_excep_located_s;\
	utilscpp_excep_located_s << "" << __FUNCTION__ \
	<< ": " << msg;\
	throw utils::Excep(utilscpp_excep_located_s.str());

#else
#define UTILS_RAISE(msg) lErr(msg)
#endif

#define UTILS_RAISE_LOCATED UTILS_RAISE("at " << __LINE__)

#define UTILS_RAISE_IF(v) \
  if(v) {UTILS_RAISE(#v " should not be ture")}

#define UTILS_RAISE_UNLESS(v) \
  UTILS_RAISE_IF(!(v))

#define UTILS_RAISE_VERBOSE_IF(v, msg) \
  if(v) {UTILS_RAISE(#v " should not be ture: " << msg)}

#define UTILS_RAISE_VERBOSE_UNLESS(v, msg) \
  UTILS_RAISE_VERBOSE_IF(!(v), msg)


/**
 * ulimit, enable core dump
 */
void EnableCoreDump() noexcept;

}
