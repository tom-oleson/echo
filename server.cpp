/*
 * Copyright (c) 2019, Tom Oleson <tom dot oleson at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * The names of its contributors may NOT be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <vector>
#include "server.h"

class watcher_store: protected cm::mutex {

protected:
    
    std::vector<int> _list;

public:

    bool add(int fd) {
        lock();
        _list.push_back(fd);
        unlock();
        return true;
    }
    
    bool remove(int fd) {
        bool removed = false;
        lock();
        for(auto it = _list.begin(); it != _list.end(); it++) {
            if((*it) == fd) {
                _list.erase(it);
                removed = true;
                break;
            }
        }
        unlock();
        return removed;   
    }

    void notify_all(cm_net::input_event *event) {
        lock();
        for(auto fd: _list) {
            if(fd != event->fd) {
                cm_net::send(fd,  event->msg);
            }
        }
        unlock();
    }

    size_t size() {
        lock();
        size_t size = _list.size();
        unlock();
        return size;
    }

    void clear() {
        lock();
        _list.clear();
        unlock();
    }
};

watcher_store watchers;

void request_handler(void *arg) {

    cm_net::input_event *event = (cm_net::input_event *) arg;

    // if this in a new connection (add to watchers list)
    if(event->connect) {
        watchers.add(event->fd);
        cm_log::info(cm_util::format("%d: socket added to watchers list: %s", event->fd, event->msg.c_str()));
        return;        
    }

    // if this in an EOF event (client disconnected)
    if(event->eof) {
        if( watchers.remove(event->fd) ) {
            cm_log::info(cm_util::format("%d: socket removed from watchers list", event->fd));
        }
        return;
    }

    cm_log::info(cm_util::format("%d: received message:", event->fd));
    cm_log::hex_dump(cm_log::level::info, event->msg.c_str(), event->msg.size(), 16);
    
    watchers.notify_all(event);
}

void request_dealloc(void *arg) {
    delete (cm_net::input_event *) arg;
}

void client_receive(int socket, const char *buf, size_t sz) {

    cm_log::info(cm_util::format("%d: received response:", socket));
    cm_log::hex_dump(cm_log::level::info, buf, sz, 16);

    cm_net::input_event event(socket, std::string(buf, sz));
    watchers.notify_all(&event);
}

void echos::run(int port, const std::string &host_name, int host_port) {

    // create thread pool that will do work for the server
    cm_thread::pool thread_pool(6);

    // startup tcp server
    cm_net::pool_server server(port, &thread_pool, request_handler,
        request_dealloc);

    cm_net::client_thread *client = nullptr;
    bool connected = false;
    if(host_port != -1) {
        cm_log::info(cm_util::format("client host: %s:%d", host_name.c_str(), host_port));
    }

    time_t next_connect_time = 0;

    while(1) {
        timespec delay = {0, 999000000};   // 999ms
        nanosleep(&delay, NULL);

        if(host_port != -1) {
            if(nullptr == client) {
                client = new cm_net::client_thread(host_name, host_port, client_receive); 
                next_connect_time = cm_time::clock_seconds() + 60;
            }

            // if client thread is running, we are connected
            if(nullptr != client && client->is_connected()) {
                if(!connected) {
                    watchers.add(client->get_socket());
                    connected = true;
                }
            }    

            // if client thread is NOT running, we are NOT connected
            if(nullptr != client && !client->is_connected()) {
                if(connected) {
                    connected = false;
                    watchers.remove(client->get_socket());
                }

                if(cm_time::clock_seconds() > next_connect_time) {
                    // attempt reconnect
                    client->start();
                    next_connect_time = cm_time::clock_seconds() + 60;
                }
            }
        }        
    }

    // wait for pool_server threads to complete all work tasks
    thread_pool.wait_all();

    if(nullptr != client) delete client;
}
