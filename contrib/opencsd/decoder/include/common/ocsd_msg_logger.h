/*!
 * \file       ocsd_msg_logger.h
 * \brief      OpenCSD : Generic Message logger / printer
 * 
 * \copyright  Copyright (c) 2015, ARM Limited. All Rights Reserved.
 */

/* 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 * may be used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */ 

#ifndef ARM_OCSD_MSG_LOGGER_H_INCLUDED
#define ARM_OCSD_MSG_LOGGER_H_INCLUDED

#include <string>
#include <fstream>

class ocsdMsgLogStrOutI
{
public:
	ocsdMsgLogStrOutI() {};
	virtual ~ocsdMsgLogStrOutI() {};

	virtual void printOutStr(const std::string &outStr) = 0;
};

class ocsdMsgLogger 
{
public:
    ocsdMsgLogger();
    ~ocsdMsgLogger();

    /** Typedef enum providing flags to define the output methods for the message logger.
    */
    typedef enum {
        OUT_NONE = 0,   /*!< No output from logger*/
        OUT_FILE = 1,   /*!< Output to file */
        OUT_STDERR = 2, /*!< Output to stderr */
        OUT_STDOUT = 4, /*!< Output to stdout */
		OUT_STR_CB = 8	/*!< output to external string callback interface */
    } output_dest;

    void setLogOpts(int logOpts); //!< set the output logging flags.
	const int getLogOpts() const  //! get the current output logging flags value. 
    { return m_outFlags; };
	
	void setLogFileName(const char *fileName);  //!< Set the output log filename, and enable logging to file.
	void setStrOutFn(ocsdMsgLogStrOutI *p_IstrOut); //!< Set the output log string callback and enable logging to callback.

    void LogMsg(const std::string &msg); //!< Log a message to the current set output channels.

    const bool isLogging() const; //!< true if logging active

private:
    int m_outFlags;

    std::string m_logFileName;
    std::fstream m_out_file;
	ocsdMsgLogStrOutI *m_pOutStrI;
};

#endif // ARM_OCSD_MSG_LOGGER_H_INCLUDED

/* End of File ocsd_msg_logger.h */
