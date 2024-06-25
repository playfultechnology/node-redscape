using System;
using System.Net;
using System.Net.Sockets;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using uPLibrary.Networking;
using uPLibrary.Networking.M2Mqtt;
using uPLibrary.Networking.M2Mqtt.Messages;
using uPLibrary.Networking.M2Mqtt.Utility;
using uPLibrary.Networking.M2Mqtt.Exceptions;
using TMPro;

namespace Playful.Technology {
    public class DeviceManager : MonoBehaviour {

        public static string GetLocalIPAddress() {
            var host = Dns.GetHostEntry(Dns.GetHostName());
            foreach (var ip in host.AddressList) {
                if (ip.AddressFamily == AddressFamily.InterNetwork) {
                    return ip.ToString();
                }
            }
            throw new Exception("No network adapters with an IPv4 address in the system!");
        }

        public Transform devicePanel;
        public GameObject devicePrefab;
        public Monitor monitor;
        private MqttClient client;

        [SerializeField]
        [Tooltip("The IP address of the broker to which this client will subscribe, e.g. 85.119.83.194")]
        private string MQTTBrokerIPAddress;

        [SerializeField]
        [Tooltip("The port on which the MQTT broker is running, e.g. 1883")]
        private int MQTTBrokerPort;

        [SerializeField]
        private string username;

        [SerializeField]
        private string password;

        [SerializeField]
        [Tooltip("The topic(s) to which to subscribe, e.g. Door/Lock/1")]
        private string[] topics;

        [SerializeField]
        private List<Device> deviceList = new List<Device>();

        Device RegisterDevice(string id) {
            Device device = new Device(id);
            return device;
        }
        
        void client_MqttMsgPublish(string topic, string msg) {
            client.Publish(topic, System.Text.Encoding.UTF8.GetBytes(msg), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
        }

        void client_MqttMsgReceived(object sender, MqttMsgPublishEventArgs e) {
            Debug.Log("Received: " + System.Text.Encoding.UTF8.GetString(e.Message));
            
            // Updates to UI need to be called on the main thread, but msgreceived callback is in a thread.
            UnityMainThreadDispatcher.Instance().Enqueue(() => {
                monitor.AddLog(System.Text.Encoding.UTF8.GetString(e.Message));
            });
            
            string[] topicArray;
            topicArray = e.Topic.Split('/');

            // e.g. ToHost/FromDevice/2
            if(topicArray.Length == 3) {
                UnityMainThreadDispatcher.Instance().Enqueue(() => {
                    monitor.AddLog(String.Format("Host received message {0} from Device {1}", System.Text.Encoding.UTF8.GetString(e.Message), topicArray[2]));
                    GameObject tmp = Instantiate(devicePrefab, devicePanel);
                    tmp.GetComponent<Device>().SetDeviceID(topicArray[2]);
                });

            }

            switch (System.Text.Encoding.UTF8.GetString(e.Message)) {
                case "Ignore":
                    break;
                default:
                    // Updates to UI need to be called on the main thread, but msgreceived callback is in a thread.
                    UnityMainThreadDispatcher.Instance().Enqueue(() => {
                        //GameObject tmp = Instantiate(devicePrefab, devicePanel);
                        //tmp.GetComponent<Device>().DeviceID = 
                    });
                    break;
            }
        }
        
        void OnGUI() {
            if (GUI.Button(new Rect(20, 40, 80, 20), "Publish")) {
                client_MqttMsgPublish("temp/random", "test message");
                monitor.AddLog(string.Format("Attempting to publish {0} to {1}", "test message", "temp/random"));
            }
            GUI.Label(new Rect(0,0,200,100),"IP Address: " + GetLocalIPAddress());
        }

        // Start is called before the first frame update
        void Start() {
            // If no MQTT broker specified, assume it's running on this machine
            if (MQTTBrokerIPAddress == "") {
                MQTTBrokerIPAddress = "127.0.0.1";
            }
            
            // Create client instance 
            client = new MqttClient(MQTTBrokerIPAddress);

            // Register the callback which will be fired when a message is received 
            client.MqttMsgPublishReceived += client_MqttMsgReceived;

            // Create a unique client ID to be associated with this connection
            string clientId = Guid.NewGuid().ToString();

            try {
                if (username != null && password != null) {
                    client.Connect(clientId, username, password);
                }
                else {
                    client.Connect(clientId);
                }
                monitor.AddLog(String.Format("Connected to MQTT server at {0} with client ID {1}", MQTTBrokerIPAddress, clientId));
            }
            catch(Exception e) {
                monitor.AddLog(e.Message);
                Debug.Log(e.Message);
            }
            // Subscribe to each topic specified
            foreach (string topic in topics) {
                client.Subscribe(new string[] { topic }, new byte[] { MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE });
                monitor.AddLog("Subscribed to " + topic);
            }
        }
   
        // Update is called once per frame
        void Update() {   }
    }
}