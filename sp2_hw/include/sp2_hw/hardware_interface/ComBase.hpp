/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Lithesh
 * All rights reserved.
 */

#pragma once

#include <iostream>
#include <thread>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <boost/function.hpp>
#include <atomic>

#define MAX_EVENTS 10

namespace ComBase
{
    /** \brief 通讯虚基类
     * \param ComProtocolT 通讯协议的数据帧数据类型，如<uint8_t>或<can_frame>
     */
    template <class ComProtocolT>
    class ComBase
    {
    public:
        ComBase() = delete;
        ComBase(const std::string &interface) : interface_name_(interface){};
        ComBase(const std::string &interface,
                boost::function<void(const ComProtocolT &rx_frame)> reception_handler)
            : interface_name_(interface), reception_handler_(std::move(reception_handler)){};
        ~ComBase();

        void setInterfaceName(const std::string &interface) { interface_name_ = interface; };
        void passRecptionHandler(boost::function<void(const ComProtocolT &rx_frame)> reception_handler)
        {
            reception_handler_ = std::move(reception_handler);
        };

        bool open(void);
        bool isOpen() { return (socket_fd_ != -1 && epoll_fd_ != -1 && receiver_thread_running_ != 1); };
        void close(void);

    protected:
        int socket_fd_ = -1;
        std::string interface_name_{};
        virtual bool openSocket(void) = 0;

    private:
        int epoll_fd_ = -1;
        epoll_event events_[MAX_EVENTS];
        std::atomic<bool> receiver_thread_running_ = false;
        std::atomic<bool> terminate_receiver_thread_ = false;

        bool openEpoll(void);
        void receiver_thread_(void);
        boost::function<void(const ComProtocolT &rx_frame)> reception_handler_;
    };

    template <class ComProtocolT>
    ComBase<ComProtocolT>::~ComBase()
    {
        if (this->isOpen())
            this->close();
    }

    template <class ComProtocolT>
    void ComBase<ComProtocolT>::close(void)
    {
        terminate_receiver_thread_ = true;
        while (receiver_thread_running_)
            ;
        if (!this->isOpen())
            return;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket_fd_, NULL);
        ::close(epoll_fd_);
        ::close(socket_fd_);
    }

    template <class ComProtocolT>
    bool ComBase<ComProtocolT>::open(void)
    {
        if (reception_handler_ == nullptr)
        {
            perror("Error reception_handler could not be empty");
            return false;
        }
        if (!(this->openSocket() && this->openEpoll()))
        {
            perror("Error creating communication");
            return false;
        }

        std::thread read_thread(&ComBase::receiver_thread_, this);
        return true;
    }

    template <class ComProtocolT>
    bool ComBase<ComProtocolT>::openEpoll(void)
    {
        if (socket_fd_ == -1)
        {
            // 防止子类没有关闭socket
            ::close(socket_fd_);
            perror("Error using file descriptor");
            return false;
        }

        epoll_event ev;
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1)
        {
            perror("Error creating epoll file descriptor");
            return false;
        }
        // 填充需检测事件描述
        ev.events = EPOLLIN;
        ev.data.fd = socket_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, ev.data.fd, &ev))
        {
            perror("Error adding socket to epoll file descriptor");
            return false;
        }

        return true;
    }

    template <class ComProtocolT>
    void ComBase<ComProtocolT>::receiver_thread_(void)
    {
        ComProtocolT rx_frame{};
        receiver_thread_running_ = true;
        while (!terminate_receiver_thread_)
        {
            // 用events存储发生了事件的file descriptor
            int num_events = epoll_wait(epoll_fd_, events_, MAX_EVENTS, -1);
            if (num_events == -1)
            {
                perror("Error while waiting for events");
                return;
            }
            // 实际上num_events肯定等于1，因为只申请了一个File Descriptor
            for (int i = 0; i < num_events; ++i)
                if (events_[i].data.fd == socket_fd_)
                {
                    ssize_t num_bytes = recv(socket_fd_, &rx_frame, sizeof(rx_frame), MSG_DONTWAIT);
                    if (num_bytes == -1)
                    {
                        perror("Error reading from SocketCan");
                        return;
                    }
                    reception_handler_(rx_frame);
                }
        }
    }
} // namespace ComBase