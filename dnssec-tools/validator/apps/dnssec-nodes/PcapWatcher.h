#ifndef PCAPWATCHER_H
#define PCAPWATCHER_H

#include <pcap.h>

#include <QThread>
#include <QTimer>

#include "DNSData.h"

class PcapWatcher : public QThread
{
    Q_OBJECT
public:
    explicit PcapWatcher(QObject *parent = 0);
    QString  deviceName();
    void     setDeviceName(const QString &deviceName);

signals:
    void     failedToOpenDevice(QString errMsg);
    void     addNode(QString nodeName);
    void     addNodeData(QString nodeName, DNSData data);

public slots:
    void     openDevice();
    void     closeDevice();
    void     processPackets();
    
private:
    void run();

    QString             m_filterString;
    struct bpf_program  m_filterCompiled;
    QString             m_deviceName;
    pcap_t             *m_pcapHandle;
    char                m_errorBuffer[PCAP_ERRBUF_SIZE];

    QTimer              m_timer;
};

#endif // PCAPWATCHER_H