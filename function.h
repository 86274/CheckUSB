#ifndef FUNCTION_H
#define FUNCTION_H

#include <QTextStream>
#include <QString>
#include <QIODevice>
#include <qt_windows.h>
#include <initguid.h>
#include <usbioctl.h>
#include <setupapi.h>
#include <devpkey.h>
#include "VendorUSB.h"
#include <QMap>

QTextStream out(stdout);

typedef struct _DEVICE_INFO_NODE {
    HANDLE                              deviceHandle = NULL; //Handle контроллера
    HDEVINFO                            deviceInfo = NULL;
    SP_DEVINFO_DATA                     deviceInfoData;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceDetailData = NULL; //Здесь указан путь к устройству
    QString                             rootHubName; //Имя концентратора Hub
    PUSB_NODE_CONNECTION_INFORMATION_EX connectionInfo = NULL;
    ULONG                               index = 0;
    ULONG                               requiredLength = 0;
} DEVICE_INFO_NODE, *PDEVICE_INFO_NODE;

typedef struct _DEVICEINFO {
    ULONG                               VendorID;
    QString                             VendorName;
    ULONG                               ProductID;
    QString                             SerialNumber;
    QString                             ProductName;
    QString                             PortHubName;
    ULONG                               PortNumber;
    ULONG                               HubNumber;

    void clear() {
        VendorID = 0;
        VendorName.clear();
        ProductID  = 0;
        SerialNumber.clear();
        ProductName.clear();
        PortHubName.clear();
        PortNumber = 0;
        HubNumber = 0;
    }
} DEVICE_INFO, *PDEVICEINFO;

ULONG parseHex(const char* s){
    const char*  p;
    ULONG n;

    for(n = 0, p = s; *p; ++p){
        if(*p >= '0' && *p <= '9')
            n = (n << 4) | (unsigned int)(*p - '0');
        else if(*p >= 'A' && *p <= 'F')
            n = (n << 4) | (unsigned int)(*p - 'A' + 10);
        else
            break;
    }

    //проверка на переполнение
    if(((int)(p - s)) > (int)(sizeof(n) << 1))
        return 0;

    return n;
}

//Парсим строку с VID, PID, Serial Number
void
parseInstanceId(DEVICE_INFO& devInfo, QString& s)
{
    QChar ch;
    QString buf; //Для перевода из 16-ричной строки в 10-тичное число
    QTextStream ss(&s, QIODevice::ReadWrite);

    while(ch != '_'){
        ss >> ch;
    }
    //Считывание VID
    ss >> ch;
    while (ch != '&') {
        buf.append(ch);
        ss >> ch;
    }

    devInfo.VendorID = parseHex(buf.toStdString().c_str());
    buf.clear();

    ss >> ch;
    while(ch != '_'){
        ss >> ch;
    }

    //Считывание PID
    ss >> ch;
    while (ch != '\\') {
        buf.append(ch);
        ss >> ch;
    }

    devInfo.ProductID = parseHex(buf.toStdString().c_str());

    while(ch != '\\'){
        ss >> ch;
    }

    ss >> ch;
    while(ch != '\0'){
        devInfo.SerialNumber.append(ch);
        ss >> ch;
    }
}

//Получение имени порта и хаба в виде строки
void
parsPortHub(DEVICE_INFO& devInfo,
            QString& s)
{
    QString buf;

    s.erase(s.begin(), s.begin() + 6);

    int i = 0;
    while(s[i] == '0') {
        ++i;
    }

    while(s[i] != '.'){
        buf.append(s[i]);
        ++i;
    }

    devInfo.PortHubName = "Port: " + buf;
    devInfo.PortNumber = buf.toULong();

    ++i;
    while(s[i] != '0') {
        ++i;
    }

    while(s[i] == '0') {
        ++i;
    }

    buf.clear();
    while(s[i] != '\0'){
        buf.append(s[i]);
        ++i;
    }
    devInfo.PortHubName += " Hub: " + buf;
    devInfo.HubNumber = buf.toULong();
}

//Получение строкового наименования производителя
QString
GetVendorString(USHORT idVendor)
{
    PVENDOR_ID vendorID = NULL;

    if (idVendor == 0x0000)
    {
        return 0;
    }

    vendorID = USBVendorIDs;

    while (vendorID->usVendorID != 0x0000)
    {
        if (vendorID->usVendorID == idVendor)
        {
            break;
        }
        vendorID++;
    }

    return (vendorID->szVendor);
}

BOOL
getDeviceInfo(DEVICE_INFO_NODE& devsInfoNode,
              DEVICE_INFO& devInfo)
{
    BOOL r = 0;
    QString s;

    s.resize(256);
    //Получение VID, PID, Serial number
    r = SetupDiGetDeviceInstanceId(devsInfoNode.deviceInfo,
                                   &devsInfoNode.deviceInfoData,
                                   reinterpret_cast<TCHAR*>(&s[0]),
            256,
            NULL);
    if (r == false)
    {
        return 0;
    }

    parseInstanceId(devInfo, s); //Достаём из строки значения VID, PID, SerialNumber

    //Получение строку с номерами порта и хаба
    s.clear();
    s.resize(256);
    r = SetupDiGetDeviceRegistryProperty(devsInfoNode.deviceInfo,
                                         &devsInfoNode.deviceInfoData,
                                         SPDRP_LOCATION_INFORMATION,
                                         NULL,
                                         reinterpret_cast<unsigned char*>(&s[0]),
            256,
            NULL);

    if (r == false)
    {
        return 0;
    }

    parsPortHub(devInfo, s); //Достаём из строки номера порта и хаба

    //Получаем наименование производителя
    devInfo.VendorName = GetVendorString(devInfo.VendorID);

    DWORD nSize;
    s.clear();
    s.resize(256);

    //Получаем имя концентратора
    r = SetupDiGetDevicePropertyW(devsInfoNode.deviceInfo,
                                  &devsInfoNode.deviceInfoData,
                                  &DEVPKEY_Device_Parent,
                                  &devsInfoNode.deviceInfoData.DevInst,
                                  reinterpret_cast<unsigned char*>(&s[0]),
            256*2,
            &nSize,
            0);

    s.erase(&s[nSize/2 - 1], s.end());
    s.replace('\\', '#');
    //Добавляем константы в начало и в конец имени концентратора
    devsInfoNode.rootHubName.clear();
    devsInfoNode.rootHubName += "\\\\?\\" + s + "#{f18a0e88-c30c-11d0-8815-00a0c906bed8}";

    return 0;

}

BOOL
ErrorGetStringDesriptor(const BOOL& success,
                        const LONG& nBytesReturned,
                        const PUSB_STRING_DESCRIPTOR&  stringDesc){
    if (!success)
    {
        return 1;
    }

    if (nBytesReturned < 2)
    {
        return 1;
    }

    if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
    {
        return 1;
    }

    if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
    {
        return 1;
    }

    if (stringDesc->bLength % 2 != 0)
    {
        return 1;
    }

    return 0;
}

VOID
getStringDescriptor(const DEVICE_INFO_NODE& devsInfoNode,
                    DEVICE_INFO& devInfo)
{
    BOOL    success = 0;
    ULONG   nBytes = 0;
    ULONG   nBytesReturned = 0;
    UCHAR   stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
            MAXIMUM_USB_STRING_LENGTH];

    PUSB_DESCRIPTOR_REQUEST stringDescReq = NULL;
    PUSB_STRING_DESCRIPTOR  stringDesc = NULL;

    nBytes = sizeof(stringDescReqBuf);

    stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
    stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq+1);

    memset(stringDescReq, 0, nBytes);

    stringDescReq->ConnectionIndex = devsInfoNode.connectionInfo->ConnectionIndex;

    stringDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
            | devsInfoNode.connectionInfo->DeviceDescriptor.iProduct;

    //Получаем строковое значение названия устройства
    success = DeviceIoControl(devsInfoNode.deviceHandle,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              stringDescReq,
                              nBytes,
                              stringDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (ErrorGetStringDesriptor(success, nBytesReturned, stringDesc)){
        devInfo.ProductName = "NoName";
    }
    else
    {
        PTCHAR p = stringDesc->bString;
        int i = 0;
        while(*p != '\0')
        {
            devInfo.ProductName.append(static_cast<QChar>(*p)) ;
            ++p;
            ++i;
        }
    }
}

VOID
printDevInfo(const DEVICE_INFO& dev)
{
    out << dev.PortHubName << '\n' << Qt::flush;

    QString B_16_PID = QString("%1").arg(dev.ProductID, 0, 16);
    QString B_16_VID = QString("%1").arg(dev.VendorID, 0, 16);
    B_16_PID = B_16_PID.toUpper();
    B_16_VID = B_16_VID.toUpper();
    B_16_PID = "0x" + B_16_PID;
    B_16_VID = "0x" + B_16_VID;

            out << "PID: " << B_16_PID << " VID: " << B_16_VID
                << " SerialNumber: " << dev.SerialNumber << '\n' << Qt::flush;

    out << "Vendor: " << dev.VendorName << '\n' << Qt::flush;

    out << "Product: " << dev.ProductName << '\n' << '\n' << Qt::flush;
}


#endif // FUNCTION_H
