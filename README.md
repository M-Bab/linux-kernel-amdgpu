## Update

My latest tries to build a mixed up kernel from the AMD code (and official linux kernel code and Ubuntu sauce) did not work well. There are massive merge conflicts and build errors which I dont have the time to figure out. But the reason for that is also kind of good news: The development in the official kernels is almost on-par with the latest AMD kernel developments.

I started this kernel because back in the days users didn't even have HDMI sound via AMD graphics card without such a special kernel. Things have improved a lot till then. The amdgpu dc code is merged for several years now and the software development there is so fast you usually get release day support for their new GPUs.

In short: This kernel is not really necessary anymore. I am not using it anymore either. I will shut these two repos down. Thanks to all users!



Linux kernel
============

There are several guides for kernel developers and users. These guides can
be rendered in a number of formats, like HTML and PDF. Please read
Documentation/admin-guide/README.rst first.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.  The formatted documentation can also be read online at:

    https://www.kernel.org/doc/html/latest/

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.
