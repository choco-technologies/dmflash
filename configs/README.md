# Configurations of dmflash

One `.ini` file is the whole configuration: there is no pin to set (the flash interface has none), only the chip and
the part of it that is exposed. `dmflash.dmr` installs this directory.

* `mcu/<mcu>.ini` - the chip: its sectors, the whole flash, **read only**. Use it to look at the flash.
* `board/<board>/flash.ini` - the same chip with a **region you may write**: the sectors after the firmware.

Select exactly one. The keys are in [docs/configuration.md](../docs/configuration.md).
