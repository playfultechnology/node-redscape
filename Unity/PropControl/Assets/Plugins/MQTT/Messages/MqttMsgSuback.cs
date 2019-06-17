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

namespace uPLibrary.Networking.M2Mqtt.Messages
{
    /// <summary>
    /// Class for SUBACK message from broker to client
    /// </summary>
    public class MqttMsgSuback : MqttMsgBase
    {
        #region Properties...

        /// <summary>
        /// Message identifier for the subscribe message
        /// that is acknowledged
        /// </summary>
        public ushort MessageId
        {
            get { return this.messageId; }
            set { this.messageId = value; }
        }

        /// <summary>
        /// List of granted QOS Levels
        /// </summary>
        public byte[] GrantedQoSLevels
        {
            get { return this.grantedQosLevels; }
            set { this.grantedQosLevels = value; }
        }

        #endregion

        // message identifier
        private ushort messageId;
        // granted QOS levels
        byte[] grantedQosLevels;

        /// <summary>
        /// Constructor
        /// </summary>
        public MqttMsgSuback()
        {
            this.type = MQTT_MSG_SUBACK_TYPE;
        }

        /// <summary>
        /// Parse bytes for a SUBACK message
        /// </summary>
        /// <param name="fixedHeaderFirstByte">First fixed header byte</param>
        /// <param name="channel">Channel connected to the broker</param>
        /// <returns>SUBACK message instance</returns>
        public static MqttMsgSuback Parse(byte fixedHeaderFirstByte, IMqttNetworkChannel channel)
        {
            byte[] buffer;
            int index = 0;
            MqttMsgSuback msg = new MqttMsgSuback();

            // get remaining length and allocate buffer
            int remainingLength = MqttMsgBase.decodeRemainingLength(channel);
            buffer = new byte[remainingLength];

            // read bytes from socket...
            channel.Receive(buffer);

            // message id
            msg.messageId = (ushort)((buffer[index++] << 8) & 0xFF00);
            msg.messageId |= (buffer[index++]);

            // payload contains QoS levels granted
            msg.grantedQosLevels = new byte[remainingLength - MESSAGE_ID_SIZE];
            int qosIdx = 0;
            do
            {
                msg.grantedQosLevels[qosIdx++] = buffer[index++];
            } while (index < remainingLength);

            return msg;
        }

        public override byte[] GetBytes()
        {
            int fixedHeaderSize = 0;
            int varHeaderSize = 0;
            int payloadSize = 0;
            int remainingLength = 0;
            byte[] buffer;
            int index = 0;

            // message identifier
            varHeaderSize += MESSAGE_ID_SIZE;

            int grantedQosIdx = 0;
            for (grantedQosIdx = 0; grantedQosIdx < this.grantedQosLevels.Length; grantedQosIdx++)
            {
                payloadSize++;
            }

            remainingLength += (varHeaderSize + payloadSize);

            // first byte of fixed header
            fixedHeaderSize = 1;

            int temp = remainingLength;
            // increase fixed header size based on remaining length
            // (each remaining length byte can encode until 128)
            do
            {
                fixedHeaderSize++;
                temp = temp / 128;
            } while (temp > 0);

            // allocate buffer for message
            buffer = new byte[fixedHeaderSize + varHeaderSize + payloadSize];

            // first fixed header byte
            buffer[index] = (byte)(MQTT_MSG_SUBACK_TYPE << MSG_TYPE_OFFSET);
            index++;

            // encode remaining length
            index = this.encodeRemainingLength(remainingLength, buffer, index);

            // message id
            buffer[index++] = (byte)((this.messageId >> 8) & 0x00FF); // MSB
            buffer[index++] = (byte)(this.messageId & 0x00FF); // LSB

            // payload contains QoS levels granted
            for (grantedQosIdx = 0; grantedQosIdx < this.grantedQosLevels.Length; grantedQosIdx++)
            {
                buffer[index++] = this.grantedQosLevels[grantedQosIdx];
            }

            return buffer;
        }

        public override string ToString()
        {
#if DEBUG
            return this.GetDebugString(
                "SUBACK",
                new object[] { "messageId", "grantedQosLevels" },
                new object[] { this.messageId, this.grantedQosLevels });
#else
            return base.ToString();
#endif
        }
    }
}
