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

#if (!MF_FRAMEWORK_VERSION_V4_2 && !MF_FRAMEWORK_VERSION_V4_3)
using System;
#else
using Microsoft.SPOT;
#endif

namespace uPLibrary.Networking.M2Mqtt.Messages
{
    /// <summary>
    /// Event Args class for subscribe request on topics
    /// </summary>
    public class MqttMsgSubscribeEventArgs : EventArgs
    {
        #region Properties...

        /// <summary>
        /// Message identifier
        /// </summary>
        public ushort MessageId
        {
            get { return this.messageId; }
            internal set { this.messageId = value; }
        }

        /// <summary>
        /// Topics requested to subscribe
        /// </summary>
        public string[] Topics
        {
            get { return this.topics; }
            internal set { this.topics = value; }
        }

        /// <summary>
        /// List of QOS Levels requested
        /// </summary>
        public byte[] QoSLevels
        {
            get { return this.qosLevels; }
            internal set { this.qosLevels = value; }
        }

        #endregion

        // message identifier
        ushort messageId;
        // topics requested to subscribe
        string[] topics;
        // QoS levels requested
        byte[] qosLevels;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="messageId">Message identifier for subscribe topics request</param>
        /// <param name="topics">Topics requested to subscribe</param>
        /// <param name="qosLevels">List of QOS Levels requested</param>
        public MqttMsgSubscribeEventArgs(ushort messageId, string[] topics, byte[] qosLevels)
        {
            this.messageId = messageId;
            this.topics = topics;
            this.qosLevels = qosLevels;
        }
    }
}
