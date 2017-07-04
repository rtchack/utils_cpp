/**
 *  Created on: Mar 9, 2016
 *      Author: xing
 */

#include "base/log.h"

#ifdef BASE_USE_LOG4CPLUS
#include <log4cplus/fileappender.h>
#include <log4cplus/consoleappender.h>
#endif

namespace base{

#ifdef BASE_USE_LOG4CPLUS
	using namespace log4cplus;

	const static std::string LOG_LAYOUT{"%d{%m%d %H:%M:%S} %p - %m\n"};

	void InitLog() {
		SharedAppenderPtr cap{new ConsoleAppender()};
		cap->setName("consolelog");
		std::unique_ptr<Layout> clay{new PatternLayout(LOG_LAYOUT)};
		cap->setLayout(std::move(clay));
		Logger r_logger = Logger::getRoot();
		r_logger.addAppender(cap);
	}

	void InitLog(const std::string log_file){
		InitLog();
		SharedAppenderPtr fap{new FileAppender(
				std::string{"/var/log/"} + log_file + std::string{".log"})};
		fap->setName("filelog");
		std::unique_ptr<Layout> flay{new PatternLayout(LOG_LAYOUT)};
		fap->setLayout(std::move(flay));
		Logger r_logger = Logger::getRoot();
		r_logger.addAppender(fap);
	}

#endif

	void CstyleLog(LogSeverity severity, const char *fmt, ...){
		char msg[BASE_MAX_LOG_MSG_LENGTH];
		va_list args;
		va_start(args, fmt);
		vsnprintf(msg, BASE_MAX_LOG_MSG_LENGTH, fmt, args);
		va_end(args);

		switch(severity){
			case LogSeverity::DBG:
#ifdef BASE_USE_LOG4CPLUS
				LOG4CPLUS_DEBUG(Logger::getRoot(), msg);
#else
				BASE_STD_LOGGER("DBG " << msg);
#endif
				return;
			case LogSeverity::INF:
#ifdef BASE_USE_LOG4CPLUS
				LOG4CPLUS_INFO(Logger::getRoot(), msg);
#else
				BASE_STD_LOGGER("INF " << msg);
#endif
				return;
			case LogSeverity::WAR:
#ifdef BASE_USE_LOG4CPLUS
				LOG4CPLUS_WARN(Logger::getRoot(), msg);
#else
				BASE_STD_LOGGER("WAR " << msg);
#endif
				return;
			case LogSeverity::ERR:
#ifdef BASE_USE_LOG4CPLUS
				LOG4CPLUS_ERROR(Logger::getRoot(), msg);
#else
				BASE_STD_LOGGER("ERR " << msg);
#endif
				return;
		}
	}

	void PrintBinary(const char *tag, const void *buf, size_t buf_len){
		char msg[1024];
		unsigned char *tmp_buf = (unsigned char *)buf;
		size_t index = 0;
		size_t i = 0;

		if(!tmp_buf) return;

#define LOG_BINARY_PUTN(fmt, ...) \
	index += snprintf(msg+index, 1000-index, fmt, ##__VA_ARGS__);\
	if(1000 <= index){\
		snprintf(msg+index, 24, "...");\
		goto print_point;\
	}

		LOG_BINARY_PUTN("[%s] {\n", tag)

		while(i<buf_len){
			LOG_BINARY_PUTN("%02x", tmp_buf[i])
			if(!(++i % 8)){
				LOG_BINARY_PUTN("  ")
			}else{
				LOG_BINARY_PUTN(" ")
			}
			if(!(i % 16)){
				LOG_BINARY_PUTN("\n")
			}
		}

		print_point:
		CstyleLog(LogSeverity::INF, "%s\n}\n", msg);
	}

}

