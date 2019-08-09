#include "vhid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)

#include <usb.h>

#define USB_HID_REPORT_TYPE_FEATURE 3
#define USBRQ_HID_GET_REPORT    0x01
#define USBRQ_HID_SET_REPORT    0x09

typedef struct{
  usb_dev_handle *dev;
  uint8_t ifnum;
}hiddevice;

/* Открыть нетипизированное USB HID устройство по VID, PID и строковым описаниям
 */
hiddevice_t* HidOpen(uint16_t vid, uint16_t pid, const wchar_t *man, const wchar_t *prod){
  struct usb_bus *bus;
  struct usb_device *dev;
  char buf[256];
  wchar_t devVendor[128], devProduct[128];
  wchar_t emptystr[] = L"";
  int len, i;
  char dispflag=0;
  usb_dev_handle *handle = NULL;
  usb_init();
  usb_find_busses();
  usb_find_devices();
  //если все вхдные значения заданы пустыми, считаем что юзер хочет получить список всех устройств
  if(man == NULL)man = emptystr;
  if(prod== NULL)prod= emptystr;
  if(vid==0 || pid==0 || man[0]==0 || prod[0]==0)dispflag=1;
  
  //перебираем все устройства на шине: сначала все шины, потом все устройства
  for(bus = usb_get_busses(); bus; bus = bus->next){
    for(dev = bus->devices; dev; dev = dev->next){
      if(!dispflag){
        //если отображение не требуется, неверные vid/pid позволяют не проверять устройство дальше
        if(dev->descriptor.idVendor != vid || dev->descriptor.idProduct != pid)continue;
      }
      //какое-то устройство нашли
      handle = usb_open(dev);
      if(handle == NULL){
        printf("Can not open\n");
        continue;
      }

      //проверяем строковые идентификаторы
      len = usb_get_string(handle, dev->descriptor.iManufacturer, 0x0409, buf, sizeof(buf));
      if(len<0){printf("0x%.4X:0x%.4X\n",dev->descriptor.idVendor, dev->descriptor.idProduct); usb_close(handle); continue;}
      //спасибо разработчикам, что вместо человеческого UTF-8 или хотя бы ACSII выбрали UFT-16LE
      //преобразуем в обычный wchar_t с потерей старшего байта (а нефиг делать не-англоязычные описания!)
      len = len/2 - 1;
      for(i=0; i<len; i++)devVendor[i]=buf[i+i+2];
      devVendor[len]=0;
      
      len = usb_get_string(handle, dev->descriptor.iProduct, 0x0409, buf, sizeof(buf));
      if(len<0){printf("0x%.4X:0x%.4X\n",dev->descriptor.idVendor, dev->descriptor.idProduct); usb_close(handle); continue;}
      len = len/2 - 1;
      for(i=0; i<len; i++)devProduct[i]=buf[i+i+2];
      devProduct[len]=0;
      
      if(dispflag){
        //если нужно только отображение - только отображаем
        printf("0x%.4X:0x%.4X\t[%ls]\t[%ls]\n",dev->descriptor.idVendor, dev->descriptor.idProduct, devVendor, devProduct);
        usb_close(handle);
      }else{
        //если же нужен именно поиск - проверяем идентификаторы, и если не совпадают - возвращаемся к поиску
        if(wcscmp(devVendor,man)!=0 || wcscmp(devProduct, prod)!=0){
          usb_close(handle);
          continue;
        }
        //нашли!
        goto dev_found;
      }
    }
  }
  return NULL; //ничего не найдено
  
//А вдруг наше устройство составное?
//Из всех доступных интерфейсов надо найти именно тот, который отвечает собственно за
//неспецифичный HID. Искать будем по тройке полей: Class, SubClass, Protocol (3.0.0)
dev_found:;
  uint8_t hid_if = -1;
  //перебираем доступные конфигурации
  for(int i=0; i< dev->descriptor.bNumConfigurations; i++){
    //для каждой из них перебираем интерфейсы
    for(int j=0; j<dev->config[i].bNumInterfaces; j++){
      //для каждого интерфейса - альтернативные конфигурации
      for(int k=0; k<dev->config[i].interface[j].num_altsetting; k++){
        struct usb_interface_descriptor *ifc = &dev->config[i].interface[j].altsetting[k];
        //проверяем является ли этот интерфейс Generic HID'ом
        if( (ifc->bInterfaceClass == 3) &&
            (ifc->bInterfaceSubClass == 0) &&
            (ifc->bInterfaceProtocol == 0) ){
          hid_if = ifc->bInterfaceNumber;
        goto interface_found;
        }//interface
      }//altsetting
    }//NumInterfaces
  }//NumConfigurations
  //ничего не нашли - выходим с позором
  usb_close(handle);
  return NULL;
  
interface_found:;
  //немного черной магии: заворачиваем device и найденный интерфейс в структуру
  hiddevice *res = (hiddevice*)malloc(sizeof(hiddevice));
  if(res == NULL){
    usb_close(handle);
    return NULL;
  }
  res->dev = handle;
  res->ifnum = hid_if;
  //Важно! Поскольку для HID уже есть стандартный драйвер, сначала выгрузим его, чтобы отдал устройство нам
  usb_detach_kernel_driver_np(handle, hid_if);
  return res;
}

void HidClose(hiddevice_t *dev){
  if(!dev)return;
  if(!((hiddevice*)dev)->dev)return;
  usb_release_interface(((hiddevice*)dev)->dev, ((hiddevice*)dev)->ifnum);
  usb_close(((hiddevice*)dev)->dev);
  free(dev);
}

char HidIsConnected(hiddevice_t *dev){
  if(dev == NULL)return 0;
  return (((hiddevice*)dev)->dev != NULL);
}

char HidWrite(hiddevice_t *dev, void *buf, size_t size){
  int bytes_send;
  //посылка HID-специфичного пакета SET_FEATURE
  bytes_send = usb_control_msg(((hiddevice*)dev)->dev,
                               USB_TYPE_CLASS | USB_ENDPOINT_OUT | USB_RECIP_INTERFACE,
                               USBRQ_HID_SET_REPORT,
                               USB_HID_REPORT_TYPE_FEATURE,
                               ((hiddevice*)dev)->ifnum,
                               buf,
                               size,
                               5000);
  if(bytes_send<0)bytes_send=0;
  return bytes_send;
}

char HidRead(hiddevice_t *dev, void *buf, size_t size){
  int bytes_recv;
  bytes_recv = usb_control_msg(((hiddevice*)dev)->dev,
                               USB_TYPE_CLASS | USB_ENDPOINT_IN | USB_RECIP_INTERFACE,
                               USBRQ_HID_GET_REPORT,
                               USB_HID_REPORT_TYPE_FEATURE,
                               ((hiddevice*)dev)->ifnum,
                               buf,
                               size,
                               5000);
  if(bytes_recv < 0)bytes_recv=0;
  return bytes_recv;
}

#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

#include <windows.h>
#include <wchar.h>
#include <setupapi.h>
#include <hidsdi.h>

static GUID hid_guid;

__attribute__((__constructor__))void whid_init(){
  HidD_GetHidGuid(&hid_guid);
}

hiddevice_t* HidOpen(uint16_t vid, uint16_t pid, const wchar_t *man, const wchar_t *prod){
  const wchar_t emptystr[]=L"";
  wchar_t w_buffer[256];
  char path[256];
  char vidpid[20]="";
  int index = 0;
  DWORD req_size = 0;
  HANDLE file;
  SP_DEVICE_INTERFACE_DATA spd;
  HDEVINFO info_set = SetupDiGetClassDevs(&hid_guid, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  char *res = NULL;
  if(man == NULL)man = emptystr;
  if(prod== NULL)prod= emptystr;
  if(man[0] != 0 && prod[0] != 0 && vid != 0 && pid != 0){
    snprintf(vidpid,18,"vid_%.4x&pid_%.4x",vid,pid);
  }
  spd.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  while(SetupDiEnumDeviceInterfaces(info_set, 0, &hid_guid, index, &spd)){
    index++;
    //GetDevicePath()
    if(!SetupDiGetDeviceInterfaceDetail(info_set, &spd, 0, 0, &req_size, 0)){
      path[0] = 0;
      SP_DEVICE_INTERFACE_DETAIL_DATA *detail = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(req_size);
      detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
      if(SetupDiGetDeviceInterfaceDetail(info_set, &spd, detail, req_size, &req_size, 0)){
        strncpy(path, detail->DevicePath, sizeof(path));
        free(detail);
      }
    }
    unsigned int cur_vid=0, cur_pid=0;
    char *tmp;
    tmp = strstr(path, "vid_");
    if(tmp != NULL)sscanf(tmp+4, "%x", &cur_vid);
    tmp = strstr(path, "pid_");
    if(tmp != NULL)sscanf(tmp+4, "%x", &cur_pid);

    //end of GetDevicePath
    if(vidpid[0]){
      res = strstr(path, vidpid);
      if(!res)continue;
    }
    file = CreateFile(path, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if(file != INVALID_HANDLE_VALUE){
      HidD_GetManufacturerString(file, w_buffer, sizeof(w_buffer));
      if(vidpid[0]){
        if(wcscmp(w_buffer,man)!=0){CloseHandle(file); continue;}
      }else{
        wprintf(L"%.4X:%.4X %ls\t",cur_vid, cur_pid, w_buffer);
      }
      HidD_GetProductString(file, w_buffer, sizeof(w_buffer));
      if(vidpid[0]){
        if(wcscmp(w_buffer,prod)!=0){CloseHandle(file); continue;}
      }else{
        wprintf(L"%ls\n",w_buffer);
      }
      CloseHandle(file);
      if(vidpid[0]){
        res = (char*)malloc(sizeof(char)*(strlen(path)+5));
        if(!res)return NULL;
        strcpy(res, path);
        SetupDiDestroyDeviceInfoList(info_set);
        return res;
      }
    }else{
      printf("%.4X:%.4X [%s]\n",cur_vid, cur_pid, path);
    }
  }
  SetupDiDestroyDeviceInfoList(info_set);
  return NULL;
}

void HidClose(hiddevice_t *dev){
  if(dev != NULL){free(dev); dev = NULL;}
}

char HidIsConnected(hiddevice_t *dev){
  if(dev == NULL)return 0;
  HANDLE file = CreateFile(dev, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
  if(file != INVALID_HANDLE_VALUE){
    CloseHandle(file);
    return 1;
  }else{
    return 0;
  }
}

char HidWrite(hiddevice_t *dev, void *buf, size_t size){
  char res;
  char buffer[size+1];
  if(dev == NULL)return 0;
  HANDLE file = CreateFile(dev, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
  if(file != INVALID_HANDLE_VALUE){
    buffer[0]=0;
    memcpy(buffer+1,buf,size);
    res = HidD_SetFeature(file, buffer, size+1);
    CloseHandle(file);
    if(res)return size;
  }
  return 0;
}

char HidRead(hiddevice_t *dev, void *buf, size_t size){
  char res;
  char buffer[size+1];
  if(dev == NULL)return 0;
  HANDLE file = CreateFile(dev, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
  if(file != INVALID_HANDLE_VALUE){
    memset(buffer,0,size+1);
    res = HidD_GetFeature(file, buffer, size+1);
    memcpy(buf,buffer+1,size);
    CloseHandle(file);
    if(res)return size;else return 0;
  }else{
    return 0;
  }
}

#else
  #error "Unsupported platform"
#endif

void HidDisplay(){HidOpen(0, 0, L"", L"");}
