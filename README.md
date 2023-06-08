# how to build and use ipcamera 
1. download sophapp (git clone https://github.com/sophgo/sophapp.git --branch=ipcamera)
2. copy sophapp to root of the release SDK
3. source build/cvisetup.sh
4. cd xxx/sophapp;make ipcamera 
5. run APP on board; example: "./ipcamera -i cv18xx_wxxb_xxx.ini &"
## Notes:
 * source build environment before build ipcam
 * most parameters can modify in *.ini files
------------------------------------------------------------------------------------
# ipcam folder tree overview
| Folder/File | Description                                             |
| ----------- | ------------------------------------------------------- |
| config      | set your build parameter                                |
| main        | app entry                                               |
| Makefile    | build app and output ipcam_cv18xx                       |
| modules     | include video audio AI OSD ...                          |
| prebuilt    | cvitek libs and 3rd libs                                |
| README.md   | notes                                                   |
| resource    | include ai models , parameter config file , and so on   |


