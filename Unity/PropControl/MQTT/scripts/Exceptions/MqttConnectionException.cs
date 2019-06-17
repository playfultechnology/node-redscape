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

namespace uPLibrary.Networking.M2Mqtt.Exceptions
{
    /// <summary>
    /// Connection to the broker exception
    /// </summary>
    public class MqttConnectionException : ApplicationException
    {
        public MqttConnectionException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }
}
