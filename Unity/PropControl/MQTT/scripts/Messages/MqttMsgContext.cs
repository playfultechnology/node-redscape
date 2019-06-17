/*
M2Mqtt Project - MQTT Client Library for .Net and GnatMQ MQTT Broker for .NET
Copyright (c) 2014, Paolo Patierno, All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.
*/

using System;
using System.Text;

namespace uPLibrary.Networking.M2Mqtt.Messages
{
    /// <summary>
    /// Context for MQTT message
    /// </summary>
    public class MqttMsgContext
    {
        /// <summary>
        /// MQTT message
        /// </summary>
        public MqttMsgBase Message { get; set; }

        /// <summary>
        /// MQTT message state
        /// </summary>
        public MqttMsgState State { get; set; }

        /// <summary>
        /// Flow of the message
        /// </summary>
        public MqttMsgFlow Flow { get; set; }

        /// <summary>
        /// Timestamp in ticks (for retry)
        /// </summary>
        public int Timestamp { get; set; }

        /// <summary>
        /// Attempt (for retry)
        /// </summary>
        public int Attempt { get; set; }
    }

    /// <summary>
    /// Flow of the message
    /// </summary>
    public enum MqttMsgFlow
    {
        /// <summary>
        /// To publish to subscribers
        /// </summary>
        ToPublish,

        /// <summary>
        /// To acknowledge to publisher
        /// </summary>
        ToAcknowledge
    }

    /// <summary>
    /// MQTT message state
    /// </summary>
    public enum MqttMsgState
    {
        /// <summary>
        /// QOS = 0, Message queued
        /// </summary>
        QueuedQos0,

        /// <summary>
        /// QOS = 1, Message queued
        /// </summary>
        QueuedQos1,

        /// <summary>
        /// QOS = 2, Message queued
        /// </summary>
        QueuedQos2,

        /// <summary>
        /// QOS = 1, PUBLISH sent, wait for PUBACK
        /// </summary>
        WaitForPuback,

        /// <summary>
        /// QOS = 2, PUBLISH sent, wait for PUBREC
        /// </summary>
        WaitForPubrec,

        /// <summary>
        /// QOS = 2, PUBREC sent, wait for PUBREL
        /// </summary>
        WaitForPubrel,

        /// <summary>
        /// QOS = 2, PUBREL sent, wait for PUBCOMP
        /// </summary>
        WaitForPubcomp,

        /// <summary>
        /// QOS = 2, start first phase handshake send PUBREC
        /// </summary>
        SendPubrec,
        
        /// <summary>
        /// QOS = 2, start second phase handshake send PUBREL
        /// </summary>
        SendPubrel,

        /// <summary>
        /// QOS = 2, end second phase handshake send PUBCOMP
        /// </summary>
        SendPubcomp,

        /// <summary>
        /// QOS = 1, PUBLISH received, send PUBACK
        /// </summary>
        SendPuback,

        /// <summary>
        /// (QOS = 1), SUBSCRIBE sent, wait for SUBACK
        /// </summary>
        WaitForSuback,

        /// <summary>
        /// (QOS = 1), UNSUBSCRIBE sent, wait for UNSUBACK
        /// </summary>
        WaitForUnsuback
    }
}
