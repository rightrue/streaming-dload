/**
*
* (c) Gassan Idriss <ghassani@gmail.com>
* 
* This file is part of streaming-dload.
* 
* streaming-dload is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* streaming-dload is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with streaming-dload. If not, see <http://www.gnu.org/licenses/>.
*
* @file streaming_dload_read_task.cpp
* @class StreamingDloadReadTask
* @package openpst/streaming-dload
* @brief Handles background processing of open/open-multi mode reading
*
* @author Gassan Idriss <ghassani@gmail.com>
*/

#include "task/streaming_dload_read_task.h"

using namespace OpenPST::GUI;

StreamingDloadReadTask::StreamingDloadReadTask(uint32_t address, size_t amount, std::string outFilePath, ProgressGroupWidget* progressContainer, StreamingDloadSerial& port) :
    address(address),
    amount(amount),
    outFilePath(outFilePath),
    progressContainer(progressContainer),
    port(port)
{

}

StreamingDloadReadTask::~StreamingDloadReadTask()
{

}

void StreamingDloadReadTask::run()
{
    QString message;
    size_t step    = amount;
    size_t total   = 0;
    int blocksRead = 0;

    if (port.state.negotiated && port.state.hello.maxPreferredBlockSize && step > port.state.hello.maxPreferredBlockSize) {
        step = port.state.hello.maxPreferredBlockSize;
    } else if (step > STREAMING_DLOAD_MAX_DATA_SIZE) {
        step = STREAMING_DLOAD_MAX_DATA_SIZE;
    }

    QMetaObject::invokeMethod(progressContainer, "setProgress",  Qt::QueuedConnection, Q_ARG(int, 0), Q_ARG(int, amount), Q_ARG(int, 0));
    QMetaObject::invokeMethod(progressContainer, "setTextLeft",  Qt::QueuedConnection, Q_ARG(QString, message.sprintf("Reading %lu bytes from 0x%08X", amount, address)));
    QMetaObject::invokeMethod(progressContainer, "setTextRight", Qt::QueuedConnection, Q_ARG(QString, message.sprintf("0/%d bytes", amount)));
    
    emit started();

    std::ofstream file(outFilePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    
    if (!file.is_open()) {
        emit error(message.sprintf("Error opening %s for writing", outFilePath.c_str()));
        return;
    }

    if (amount % 512 != 0) {
        while(amount % 512 != 0) {
            amount++;
        }
        emit log(message.sprintf("Adjusted read amount to %lu for block size alignment to %d bytes per sector", amount, 512));
    }

    emit log(message.sprintf("Reading %lu bytes from LBA %08X (%d sectors) Writing data to %s", 
        amount, 
        address,
        amount/512,
        outFilePath.c_str()
    ));

    while (total < amount) {
        if (cancelled()) {
            log("Aborted Read");
            emit aborted();
            return;
        }

        if ((amount - total) < step) {
            step = amount - total;
        }

        try {
            size_t thisRead = port.readFlash(address + blocksRead, step, file);

            total += thisRead;
            blocksRead += thisRead/512;
        } catch(StreamingDloadSerialError& e) {
            file.close();
            emit error(message.sprintf("Error reading %lu bytes from LBA 0x%08X: %s", step, (address + total), e.what()));
            return;
        } catch(SerialError& e) {
            file.close();
            emit error(message.sprintf("Error reading %lu bytes from LBA 0x%08X: %s", step, (address + total), e.what()));
            return;
        } catch (...) {
            file.close();
            emit error("Unexpected error encountered");
            return;
        }


        QMetaObject::invokeMethod(progressContainer, "setProgress", Qt::QueuedConnection, Q_ARG(int, total));
        QMetaObject::invokeMethod(progressContainer, "setTextRight", Qt::QueuedConnection, Q_ARG(QString, message.sprintf("%d/%d bytes", total, amount)));
    }

    emit log(message.sprintf("Read %lu bytes from 0x%08X. Contents saved to %s", amount, address, outFilePath.c_str()));

    file.close();

    emit complete();

}