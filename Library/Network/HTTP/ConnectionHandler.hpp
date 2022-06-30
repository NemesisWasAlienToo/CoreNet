#pragma once

#include <string>
#include <tuple>
#include <memory>
#include <variant>
#include <future>

#include <Event.hpp>
#include <Duration.hpp>
#include <ePoll.hpp>
#include <Iterable/List.hpp>
#include <Network/HTTP/HTTP.hpp>
#include <Format/Stream.hpp>
#include <Network/HTTP/Response.hpp>
#include <Network/HTTP/Request.hpp>
#include <Network/HTTP/Parser.hpp>
#include <Network/TCPServer.hpp>
#include <Network/HTTP/Router.hpp>
#include <File.hpp>

namespace Core
{
    namespace Network
    {
        namespace HTTP
        {
            struct ConnectionSettings
            {
                size_t MaxHeaderSize;
                size_t MaxBodySize;
                size_t MaxFileSize;
                size_t SendFileThreshold;
                size_t RequestBufferSize;
                size_t ResponseBufferSize;
                std::string HostName;
                std::function<void(Network::EndPoint const &, Network::HTTP::Response &, std::shared_ptr<void> &)> OnError;
            };

            template <typename TCallback>
            struct ConnectionHandler
            {
                struct OutEntry
                {
                    Iterable::Queue<char> Buffer;
                    std::shared_ptr<File> FilePtr;
                    size_t FileContentLength;
                    bool SendFile;
                };

                Duration Timeout;
                Network::EndPoint Target;
                Iterable::Queue<HTTP::Request> IBuffer;
                Iterable::Queue<OutEntry> OBuffer;
                ConnectionSettings const &Settings;
                TCallback OnRequest;

                // @todo Fix this limitations
                HTTP::Parser Parser{Settings.MaxHeaderSize, Settings.MaxBodySize, Settings.RequestBufferSize};
                bool ShouldClose = false;

                ConnectionHandler(Duration const &Timeout, Network::EndPoint const &Target, ConnectionSettings const &settings, TCallback &&CB)
                    : Timeout(Timeout),
                      Target(Target),
                      Settings(settings),
                      OnRequest(std::forward<TCallback>(CB))
                {
                }

                void AppendResponse(HTTP::Response &Response)
                {
                    // Handle keep-alive

                    if (!ShouldClose && Response.Version == HTTP::HTTP10)
                    {
                        Response.Headers.insert_or_assign("Connection", "keep-alive");
                    }
                    else if (ShouldClose && Response.Version == HTTP::HTTP11)
                    {
                        Response.Headers.insert_or_assign("Connection", "close");
                    }

                    bool HasFile = std::holds_alternative<std::shared_ptr<File>>(Response.Content);
                    size_t FileLength = 0;
                    size_t StringLength = 0;

                    HasFile ? FileLength =
                                  std::min(std::get<std::shared_ptr<File>>(Response.Content)->Size(), Settings.MaxFileSize)
                            : StringLength = std::min(std::get<std::string>(Response.Content).length(), Settings.MaxBodySize);

                    bool UseSendFile = Settings.SendFileThreshold && HasFile && FileLength > Settings.SendFileThreshold;

                    // Trim content if its too big

                    Response.Headers.insert_or_assign("Content-Length", std::to_string(HasFile ? FileLength : StringLength));
                    Response.Headers.insert_or_assign("Host", Settings.HostName);

                    OBuffer.Add(
                        Iterable::Queue<char>(Settings.ResponseBufferSize),
                        // @todo Remove pointer
                        HasFile ? std::get<std::shared_ptr<File>>(Response.Content) : nullptr,
                        FileLength,
                        UseSendFile);

                    Format::Stream Ser(OBuffer.Tail().Buffer);

                    Ser << Response;
                }

                void Pause(Async::EventLoop *Loop, Async::EventLoop::Entry &Self)
                {
                    Loop->Modify(Self, ePoll::Out);
                }

                void Resume(Async::EventLoop *Loop, Async::EventLoop::Entry &Self)
                {
                    Loop->Reschedual(Self, Timeout);
                }

                void operator()(Async::EventLoop *Loop, ePoll::Entry &Item, Async::EventLoop::Entry &Self)
                {
                    Network::Socket &Client = *static_cast<Network::Socket *>(&Self.File);

                    if (Item.Happened(ePoll::In) | Item.Happened(ePoll::UrgentIn))
                    {
                        if (!Client.Received())
                        {
                            Loop->Remove(Self.Iterator);
                            return;
                        }

                        Loop->Reschedual(Self, Timeout);

                        OnRead(Loop, Self);
                    }

                    if (Item.Happened(ePoll::Out))
                    {
                        OnWrite(Loop, Self);
                    }

                    if (Item.Happened(ePoll::HangUp) || Item.Happened(ePoll::Error))
                    {
                        Loop->Remove(Self.Iterator);
                    }
                }

                void OnRead(Async::EventLoop *Loop, Async::EventLoop::Entry &Self)
                {
                    Network::Socket &Client = *static_cast<Network::Socket *>(&Self.File);

                    try
                    {
                        Parser(Client);

                        if (Parser.IsFinished())
                        {
                            // Process request

                            Network::HTTP::Response Response = OnRequest(Target, Parser.Result, Loop->Storage);
                            // Network::HTTP::Response Response = OnRequest(Loop, Self, Target, Parser.Result);

                            // Decide if we should keep the connection

                            auto It = Parser.Result.Headers.find("Connection");
                            auto End = Parser.Result.Headers.end();

                            // @todo Optimize this

                            {
                                std::string ConnectionValue;

                                if (It != End)
                                {
                                    // @todo Optimize this

                                    ConnectionValue.resize(It->second.length());

                                    std::transform(
                                        It->second.begin(),
                                        It->second.end(),
                                        ConnectionValue.begin(),
                                        [](auto c)
                                        {
                                            return std::tolower(c);
                                        });
                                }

                                // @todo Optimize this

                                if (Parser.Result.Version == HTTP::HTTP10 && ConnectionValue != "keep-alive")
                                {
                                    ShouldClose = true;
                                }
                                else if (Parser.Result.Version == HTTP::HTTP11 && ConnectionValue == "close")
                                {
                                    ShouldClose = true;
                                }
                            }

                            // If we should close the connection, stop reading data from client

                            if (ShouldClose)
                            {
                                Client.ShutDown(Network::Socket::Read);
                            }

                            // Append response to buffer

                            AppendResponse(Response);

                            // Modify events

                            Loop->Modify(Self, ShouldClose ? ePoll::Out : ePoll::In | ePoll::Out);

                            // Reset Parser

                            Parser.Reset();
                        }
                    }
                    catch (HTTP::Response &Response)
                    {
                        if (Settings.OnError)
                            Settings.OnError(Target, Response, Loop->Storage);

                        AppendResponse(Response);

                        // Set tcp no push if enabled bu user

                        Loop->Modify(Self, ShouldClose ? ePoll::Out : ePoll::In | ePoll::Out);

                        ShouldClose = true;
                    }
                    // catch (HTTP::Status Method)
                    // {
                    //     auto Response = HTTP::Response::From(Parser.Result.Version.empty() ? HTTP10 : Parser.Result.Version, Method, {{"Connection", "close"}}, "");

                    //     if (Settings.OnError)
                    //         Settings.OnError(Target, Response, Loop->Storage);

                    //     AppendResponse(Response);

                    //     Loop->Modify(Self, ShouldClose ? ePoll::Out : ePoll::In | ePoll::Out);

                    //     ShouldClose = true;
                    // }
                }

                void OnWrite(Async::EventLoop *Loop, Async::EventLoop::Entry &Self)
                {
                    Network::Socket &Client = *static_cast<Network::Socket *>(&Self.File);

                    // If there is nothing to send

                    if (OBuffer.IsEmpty())
                    {
                        if (ShouldClose)
                        {
                            Loop->Remove(Self.Iterator);
                            return;
                        }

                        OBuffer.Free();

                        Loop->Modify(Self, ePoll::In);
                        return;
                    }

                    auto &Item = OBuffer.Head();
                    Format::Stream Ser(Item.Buffer);

                    // Append file content

                    if (Item.FileContentLength && !Item.SendFile)
                    {
                        Item.FileContentLength -= Ser.ReadOnce(*Item.FilePtr, Item.FileContentLength);
                    }

                    // Send data in buffer

                    if (!Item.Buffer.IsEmpty())
                    {
                        // Write data

                        // @todo Make socket non-blocking and improve this

                        Self.File << Ser;

                        if (!Item.Buffer.IsEmpty())
                            return;
                    }

                    // Send file

                    if (Item.FileContentLength && Item.SendFile)
                    {
                        Item.FileContentLength -= Client.SendFile(*Item.FilePtr, Item.FileContentLength);
                    }

                    // Pop buffer if we're done

                    if (Item.Buffer.IsEmpty() && !Item.FileContentLength)
                    {
                        OBuffer.Take();
                    }
                }
            };
        }
    }
}