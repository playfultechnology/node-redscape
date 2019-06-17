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

#if SSL
#if (MF_FRAMEWORK_VERSION_V4_2 || MF_FRAMEWORK_VERSION_V4_3)
using Microsoft.SPOT.Net.Security;
#else
using System.Net.Security;
using System.Security.Authentication;
#endif
#endif
using System.Net.Sockets;
using System.Net;
using System.Security.Cryptography.X509Certificates;

namespace uPLibrary.Networking.M2Mqtt
{
    /// <summary>
    /// Channel to communicate over the network
    /// </summary>
    public class MqttNetworkChannel : IMqttNetworkChannel
    {
        // remote host information
        private string remoteHostName;
        private IPAddress remoteIpAddress;
        private int remotePort;

        // socket for communication
        private Socket socket;
        // using SSL
        private bool secure;

        // CA certificate
        private X509Certificate caCert;

        /// <summary>
        /// Remote host name
        /// </summary>
        public string RemoteHostName { get { return this.remoteHostName; } }

        /// <summary>
        /// Remote IP address
        /// </summary>
        public IPAddress RemoteIpAddress { get { return this.remoteIpAddress; } }

        /// <summary>
        /// Remote port
        /// </summary>
        public int RemotePort { get { return this.remotePort; } }

#if SSL
        // SSL stream
        private SslStream sslStream;
#if (!MF_FRAMEWORK_VERSION_V4_2 && !MF_FRAMEWORK_VERSION_V4_3)
        private NetworkStream netStream;
#endif
#endif

        /// <summary>
        /// Data available on the channel
        /// </summary>
        public bool DataAvailable
        {
            get
            {
#if SSL
#if (MF_FRAMEWORK_VERSION_V4_2 || MF_FRAMEWORK_VERSION_V4_3)
                if (secure)
                    return this.sslStream.DataAvailable;
                else
                    return (this.socket.Available > 0);
#else
                if (secure)
                    return this.netStream.DataAvailable;
                else
                    return (this.socket.Available > 0);
#endif
#else
                return (this.socket.Available > 0);
#endif
            }
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="socket">Socket opened with the client</param>
        public MqttNetworkChannel(Socket socket)
        {
            this.socket = socket;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="remoteHostName">Remote Host name</param>
        /// <param name="remoteIpAddress">Remote IP address</param>
        /// <param name="remotePort">Remote port</param>
        public MqttNetworkChannel(string remoteHostName, IPAddress remoteIpAddress, int remotePort) :
            this(remoteHostName, remoteIpAddress, remotePort, false, null)
        {
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="remoteHostName">Remote Host name</param>
        /// <param name="remoteIpAddress">Remote IP address</param>
        /// <param name="remotePort">Remote port</param>
        /// <param name="secure">Using SSL</param>
        /// <param name="caCert">CA certificate</param>
        public MqttNetworkChannel(string remoteHostName, IPAddress remoteIpAddress, int remotePort, bool secure, X509Certificate caCert)
        {
            this.remoteHostName = remoteHostName;
            this.remoteIpAddress = remoteIpAddress;
            this.remotePort = remotePort;
            this.secure = secure;
            this.caCert = caCert;
        }

        /// <summary>
        /// Connect to remote server
        /// </summary>
        public void Connect()
        {
            this.socket = new Socket(this.remoteIpAddress.GetAddressFamily(), SocketType.Stream, ProtocolType.Tcp);
            // try connection to the broker
            this.socket.Connect(new IPEndPoint(this.remoteIpAddress, this.remotePort));

#if SSL
            // secure channel requested
            if (secure)
            {
                // create SSL stream
#if (MF_FRAMEWORK_VERSION_V4_2 || MF_FRAMEWORK_VERSION_V4_3)
                this.sslStream = new SslStream(this.socket);
#else
                this.netStream = new NetworkStream(this.socket);
                this.sslStream = new SslStream(this.netStream);
#endif

                // server authentication (SSL/TLS handshake)
#if (MF_FRAMEWORK_VERSION_V4_2 || MF_FRAMEWORK_VERSION_V4_3)
                this.sslStream.AuthenticateAsClient(this.remoteHostName,
                    null,
                    new X509Certificate[] { this.caCert },
                    SslVerification.CertificateRequired,
                    SslProtocols.TLSv1);
#else
                this.sslStream.AuthenticateAsClient(this.remoteHostName,
                    new X509CertificateCollection(new X509Certificate[] { this.caCert }),
                    SslProtocols.Tls, false);
#endif
            }
#endif
        }

        /// <summary>
        /// Send data on the network channel
        /// </summary>
        /// <param name="buffer">Data buffer to send</param>
        /// <returns>Number of byte sent</returns>
        public int Send(byte[] buffer)
        {
#if SSL
            if (this.secure)
            {
                this.sslStream.Write(buffer, 0, buffer.Length);
                return buffer.Length;
            }
            else
                return this.socket.Send(buffer, 0, buffer.Length, SocketFlags.None);
#else
            return this.socket.Send(buffer, 0, buffer.Length, SocketFlags.None);
#endif
        }

        /// <summary>
        /// Receive data from the network
        /// </summary>
        /// <param name="buffer">Data buffer for receiving data</param>
        /// <returns>Number of bytes received</returns>
        public int Receive(byte[] buffer)
        {
#if SSL
            if (this.secure)
            {
                // read all data needed (until fill buffer)
                int idx = 0;
                while (idx < buffer.Length)
                {
                    idx += this.sslStream.Read(buffer, idx, buffer.Length - idx);
                }
                return buffer.Length;
            }
            else
            {
                // read all data needed (until fill buffer)
                int idx = 0;
                while (idx < buffer.Length)
                {
                    idx += this.socket.Receive(buffer, idx, buffer.Length - idx, SocketFlags.None);
                }
                return buffer.Length;
            }
#else
            // read all data needed (until fill buffer)
            int idx = 0;
            while (idx < buffer.Length)
            {
                idx += this.socket.Receive(buffer, idx, buffer.Length - idx, SocketFlags.None);
            }
            return buffer.Length;
#endif
        }

        /// <summary>
        /// Close the network channel
        /// </summary>
        public void Close()
        {
#if SSL
            if (this.secure)
            {
#if (!MF_FRAMEWORK_VERSION_V4_2 && !MF_FRAMEWORK_VERSION_V4_3)
                this.netStream.Close();
#endif
                this.sslStream.Close();
            }
            this.socket.Close();
#else
            this.socket.Close();
#endif
        }
    }

    /// <summary>
    /// IPAddress Utility class
    /// </summary>
    public static class IPAddressUtility
    {
        /// <summary>
        /// Return AddressFamily for the IP address
        /// </summary>
        /// <param name="ipAddress">IP address to check</param>
        /// <returns>Address family</returns>
        public static AddressFamily GetAddressFamily(this IPAddress ipAddress)
        {
#if (!MF_FRAMEWORK_VERSION_V4_2 && !MF_FRAMEWORK_VERSION_V4_3)
            return ipAddress.AddressFamily;
#else
            return (ipAddress.ToString().IndexOf(':') != -1) ? 
                AddressFamily.InterNetworkV6 : AddressFamily.InterNetwork;
#endif
        }
    }
}
