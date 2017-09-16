#ifndef __MYHID_H__
#define __MYHID_H__

#include <wchar.h>

//реальный тип зависит от реализации, так что использовать только функции из этой библиотеки
typedef void* hiddevice_t;
#define hiddevice_def NULL

/*   Поиск и инициализация устройства по заданным параметрам. Если хотя бы один параметр не задан
 * то идет просто вывод на stdout обнаруженных устройств.
 *   Входные параметры:
 * vid, pid - vendor ID, product ID устройства
 * man[] - Manufacturer, производитель. В формате wchar_t, то есть константу можно передать как L"Manufacturer" например
 * prod[] - Product, название устройства. Аналогично man[]
 *   Выходное значение:
 * hiddevice_t, указатель на область памяти, выделенный данной функцией. В случае ошибки - NULL.
 * Это значение надо ОБЯЗАТЕЛЬНО сохранить и в конце освободить в функции HidCloseDevice()
 */
hiddevice_t HidOpenDevice(int vid, int pid, const wchar_t man[], const wchar_t prod[]);
/*  Закрытие открытого ранее устройства */
void HidCloseDevice(hiddevice_t dev);
/*  Проверка удачно ли открылось устройство (bool) */
char HidIsConnected(hiddevice_t dev);
/*  Передача массива данных в открытое устройство */
char HidSendArr(hiddevice_t dev, void *buf, size_t size);
/*  Прием массива данных заданного размера из устройства */
char HidReceiveArr(hiddevice_t dev, void *buf, size_t size);

#endif
