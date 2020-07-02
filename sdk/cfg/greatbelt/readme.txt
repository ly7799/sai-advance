$Id: README,v 1.0 Centec SDK $
This file explains how to use datapath cfg correctly.
Datapath file is important for chip init, for different hardware platform must use
corresponding datapath cfg file.
For use correct datapath file, you must rename the corresponding configure file to
datapath_cfg.txt, and put it at the same directory path as SDK image.
If you use the wrong datapath cofigure file will cause unpredictable result. If you
want to change datapath configure file, you need reboot system or delete /tmp/sdk_init
then reinit SDK.
