using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;

namespace Playful.Technology {

    // A device is the base class from which all prop controllers derive
    [Serializable]
    public class Device : MonoBehaviour {

        [SerializeField]
        private GameObject idPanel;

        private void Start() {
            
        }
        private void Update() {
            
        }



        public Device() {
            Debug.Log("new Device created");
        }
        public Device(string id) {
            SetDeviceID(id);
            Debug.Log(String.Format("new Device created with id {0}", id));
        }/*
        public Device(string name) {
            SetDeviceID(UnityEngine.Random.Range(1, int.MaxValue));
            _deviceName = name;
            Debug.Log("new Device created");
        }*/
        public Device(string id, string name) {
            SetDeviceID(id);
            _deviceName = name;
            Debug.Log("new Device created");
        }

        // Unique device ID
        private string _deviceID;
        public string GetDeviceID() { return _deviceID; }
        public void SetDeviceID(string id) {
            _deviceID = id; idPanel.GetComponent<TMPro.TextMeshProUGUI>().text = id;
        }
        /*
        public int DeviceID {
            get { return _deviceID; }
            public set { _deviceID = value; idPanel.GetComponent<TMPro.TextMeshProUGUI>().text = value.ToString(); }
        }
        */
        // Friendly device name
        private string _deviceName;
        public string DeviceName {
            get { return _deviceName; }
            private set { _deviceName = value; }
        }

        // Timestamp at which communication from device was last received
        private float _lastKnownTime;
        public float LastKnownTime {
            get { return _lastKnownTime; }
            private set { _lastKnownTime = value; }
        }

        // The last reported state of the device
        private int _lastKnownState;
        public int LastKnownState {
            get { return _lastKnownState; }
            private set { _lastKnownState = value; }
        }
    }
}