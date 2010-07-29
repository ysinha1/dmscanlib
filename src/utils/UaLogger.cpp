/*
Dmscanlib is a software library and standalone application that scans 
and decodes libdmtx compatible test-tubes. It is currently designed 
to decode 12x8 pallets that use 2D data-matrix laser etched test-tubes.
Copyright (C) 2010 Canadian Biosample Repository

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "UaLogger.h"

#if defined (WIN32) && ! defined(__MINGW32__)
#include <time.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <fstream>
#include <string>
#include <sstream>

#ifdef UA_LOGGER

using namespace std;
using namespace ua;

LoggerStringBuf<char> ua::logstream;
std::ostream ua::cdebug (&ua::logstream);

LoggerImpl::LoggerImpl() {
    std::ostringstream header;

    headers_m.resize(maxSubSys_m);

    for (unsigned i = 0; i < maxSubSys_m; ++i) {
        levels_am[i] = 0;
        header << "SYS_" << i;
        headers_m[i] = header.str();
        header.str("");
    }
}

void LoggerImpl::levelInc(unsigned subsys) {
    assert(subsys <= allSubSys_m);

    if (subsys == allSubSys_m) {
        for (unsigned i = 0; i < maxSubSys_m; ++i) {
            if (levels_am[i] < maxLevel_m)
                ++levels_am[i];
        }
        return;
    }

    if (levels_am[subsys] < maxLevel_m)
        ++levels_am[subsys];
}

void LoggerImpl::levelDec(unsigned subsys) {
    assert(subsys <= allSubSys_m);

    if (subsys == allSubSys_m) {
        for (unsigned i = 0; i < maxSubSys_m; ++i) {
            if (levels_am[i] > 0)
                --levels_am[i];
        }
        return;
    }

    if (levels_am[subsys] > 0)
        --levels_am[subsys];
}

void LoggerImpl::levelSet(unsigned subsys, unsigned level) {
    assert(subsys <= allSubSys_m);
    assert(level <= maxLevel_m);

    if (subsys == allSubSys_m) {
        for (unsigned i = 0; i < maxSubSys_m; ++i) {
            levels_am[i] = level;
        }
        return;
    }

    if (levels_am[subsys] < maxLevel_m)
        levels_am[subsys] = level;
}

unsigned LoggerImpl::levelGet(unsigned subsys) {
    assert(subsys < allSubSys_m);
    return levels_am[subsys];
}

void LoggerImpl::subSysHeaderSet(unsigned subsys, std::string header) {
    assert(subsys < allSubSys_m);
    headers_m[subsys] = header;
}

std::string & LoggerImpl::subSysHeaderGet(unsigned subsys) {
    assert(subsys < allSubSys_m);
    return headers_m[subsys];
}


// Turns off all debugging
void LoggerImpl::reset() {
    for (unsigned i = 0; i < maxSubSys_m; ++i) {
        levels_am[i] = 0;
    }
}

// Returns true if this level is enabled for debugging.
bool LoggerImpl::isDebug (unsigned subsys, unsigned level) {
    assert(subsys < allSubSys_m);
    return (level <= levels_am[subsys]);
}

void LoggerSink::standardHeader(std::string & str_r) {
    char buf_a[100];

#if defined (WIN32) && ! defined(__MINGW32__)
   time_t ltime;
   struct _timeb tstruct;
   char timebuf[26];
   errno_t err;

   time( &ltime );
   err = ctime_s(timebuf, 26, &ltime);
   if (err) {
      cerr << "ctime_s failed due to an invalid argument.";
      exit(1);
   }
   _ftime_s( &tstruct );
   sprintf_s(buf_a, "%.8s:%03u ", timebuf + 11, tstruct.millitm);
#else
    // Fetch the current time
    char time_a[100];
    struct timeval thistime;

    gettimeofday(&thistime, NULL);
    strftime(time_a, sizeof(time_a), "%X", localtime(&thistime.tv_sec));
    snprintf(buf_a, sizeof (buf_a), "%s:%03ld ", time_a,
             thistime.tv_usec / 1000);
#endif

    str_r = buf_a;
}

void LoggerSinkStdoutImpl::write(const std::string& str_r) {
	mutex.lock();
    if (str_r.size() == 0)
        return;

    std::string op;

    if (enableHeader_m)
        header(op);

    op += str_r;

    // Add a trailing newline if we don't have one
    // (we need this when we shut down)
    if (op[op.length()-1] != '\n')
        op += '\n';

    std::cout << op;
    mutex.unlock();
}

void LoggerSinkFileImpl::write(const std::string& str_r) {
	mutex.lock();
    if (file_m.size() == 0)
        return;

    if (str_r.size() == 0)
        return;

    std::string op;

    if (enableHeader_m)
        header(op);

    op += str_r;

    // Open the file in append mode. The dtor will close
    // the file for us.
    std::ofstream output(file_m.c_str(), std::ios_base::app);
    if (!output)
        return;    // The file could not be opened. Exit

    output << op;
    mutex.unlock();
}


void LoggerSinkFileImpl::setFile(const std::string& file_r) {
    file_m = file_r;
}

#endif /* DEBUG */
