using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class Monitor : MonoBehaviour
{
    public ScrollRect scrollRect;
    public TextMeshProUGUI tmProUGUI;


    struct LogEntry {
        public string message;
        public string timestamp;
    }
    List<LogEntry> log = new List<LogEntry>();
       
    public void AddLog(string msg) {
        log.Add(new LogEntry() {
            message = msg,
            timestamp = System.DateTime.UtcNow.ToString("HH:mm:ss dd/MM/yy"),
        });

        RefreshLog();
    }

    void RefreshLog() {
        tmProUGUI.text = "";
        for (int i=log.Count-1; i>=0; i--) {
            tmProUGUI.text += log[i].timestamp + "," + log[i].message + "\n";
        }
        scrollRect.verticalNormalizedPosition = 1f;
    }


    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
