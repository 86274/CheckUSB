#include <QCoreApplication>
#include "function.h"

int main(int argc, char* argv[]){
    setlocale(LC_ALL,"Russian");

    QCoreApplication a(argc, argv);

    BOOL r = 1; //Для проверки успешности выполнения функции
    DWORD i = 0; //Индекс устройства
    DEVICE_INFO_NODE devsInfoNode; //Структура с необходимыми переменными для доступа к необходимой информации
    DEVICE_INFO devInfo; // Структура с необоходимой информацией об устройстве
    QMap<QString, DEVICE_INFO> devsMap;//Подключённые устройства на предыдущем проходе
    QMap<QString, DEVICE_INFO> devsMapBuf;//Подключённые устройства на текущем проходе

    while(true) {
        //Получение списка подключенных USB
        devsInfoNode.deviceInfo =  SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE,
                                                       NULL,
                                                       NULL,
                                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (devsInfoNode.deviceInfo == INVALID_HANDLE_VALUE) {
            out << "hdevinfo" << GetLastError() << '\n' << Qt::flush;
        }
        else
        {

            devsInfoNode.deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            //Получение информации о каждом устройстве в списке
            while(SetupDiEnumDeviceInfo(devsInfoNode.deviceInfo,
                                        i,
                                        &devsInfoNode.deviceInfoData)) {

                devsInfoNode.deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

                SetupDiEnumDeviceInterfaces(devsInfoNode.deviceInfo,
                                            NULL,
                                            &GUID_DEVINTERFACE_USB_DEVICE,
                                            i,
                                            &devsInfoNode.deviceInterfaceData);

                //Получение размера буфера
                r = SetupDiGetDeviceInterfaceDetail(devsInfoNode.deviceInfo,
                                                    &devsInfoNode.deviceInterfaceData,
                                                    nullptr,
                                                    NULL,
                                                    &devsInfoNode.requiredLength,
                                                    nullptr);

                devsInfoNode.deviceDetailData = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>
                        (malloc(devsInfoNode.requiredLength));
                devsInfoNode.deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                //Получение пути к устройству
                r = SetupDiGetDeviceInterfaceDetail(devsInfoNode.deviceInfo,
                                                    &devsInfoNode.deviceInterfaceData,
                                                    devsInfoNode.deviceDetailData,
                                                    devsInfoNode.requiredLength,
                                                    &devsInfoNode.requiredLength,
                                                    nullptr);
                if (r == 0){
                    out << "SetupDiGetDeviceInterfaceDetail " << GetLastError() << '\n' << Qt::flush;
                }

                //Получение всей необходимой информации кроме имени устройства
                getDeviceInfo(devsInfoNode, devInfo);

                //Открываем концентратор
                devsInfoNode.deviceHandle = CreateFile
                        (reinterpret_cast<PTCHAR>(&devsInfoNode.rootHubName[0]),
                        GENERIC_WRITE,
                        FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        NULL);

                if (INVALID_HANDLE_VALUE ==  devsInfoNode.deviceHandle) {
                    out << "CreateFileHub " << GetLastError() << '\n' << Qt::flush;
                    return 1;
                }

                ULONG nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;
                devsInfoNode.connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION_EX)malloc(nBytes);
                devsInfoNode.connectionInfo->ConnectionIndex = devInfo.PortNumber;

                //Отправляем запрос драйверу устройства для получения дескриптора устройства и прочей информации
                r = DeviceIoControl(devsInfoNode.deviceHandle,
                                    IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                    devsInfoNode.connectionInfo,
                                    nBytes,
                                    devsInfoNode.connectionInfo,
                                    nBytes,
                                    &nBytes,
                                    NULL);

                if (!r)
                {
                    out << GetLastError() << '\n' << Qt::flush;
                    continue;
                }

                //Получаем имя устройство в виде строки
                getStringDescriptor(devsInfoNode, devInfo);

                devsMapBuf.insert(devInfo.PortHubName, devInfo);

                devInfo.clear();
                CloseHandle(devsInfoNode.deviceHandle);
                free(devsInfoNode.deviceDetailData);

                ++i;

            }
        }

        //Проверяем подключилось ли новое устройство
        for(const auto& i : devsMapBuf.keys())
        {
            if (!devsMap.count(i))
            {
                printDevInfo(devsMapBuf[i]);
            }
        }

        devsMap = devsMapBuf; //Обновляем список подключённых устройств
        devsMapBuf.clear();
        i = 0;
        SetupDiDestroyDeviceInfoList (devsInfoNode.deviceInfo);
        Sleep(250); //Чтобы хост отвечал на все запросы других устройств USB
    }

    return a.exec();
}
