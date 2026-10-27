# flashtest

On-board test of the dmflash block device node, e.g. `/dev/dmflashx/flash`.

```
flashtest <node>
```

It works on the **last sector of the region** the node exposes (the region has to be `mode=rw`, see
`configs/board/*/flash.ini`): the content of the sector is saved first and written back at the end - a failure in
between can still leave it erased. The sector is erased, written, read back through the node and through the
address of the flash, a write over data that is not erased and misaligned erases are refused. Needs a heap block of
twice the sector's size (2 x 256 kB on an STM32F7).

The exit code is the number of failed steps. The CPU stops while a sector is erased (a second at most for 256 kB).
