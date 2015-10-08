Environment Setup
=================

1. These directories must exist on the target:
    /opt/rc360/apps/GE/
    /opt/rc360/apps/TPA/
    /opt/rc360/modules/GE/
    /opt/rc360/modules/TPA/
    /opt/rc360/system/

2. AACM should be the only executable in the /opt/rc360/system/ folder.

3. BARSM should be started, it does not have to be located anywhere specific

4. BARSM will launch any executable in the specified directories, starting with AACM in the system folder.


How To Start FPGA Sim
=====================

1. A simulation folder must be created on the target:
    /opt/rc360/simult/ 

2. The FPGA sim must be started before SIMM is launched by BARSM, to ensure this start it before BARSM.  The "fpga" executable can be located anywhere, we usually place it in the /opt/rc360/simult/ folder.


Known Issues
============

None

