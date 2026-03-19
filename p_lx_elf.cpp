// For a shared library, then the system runtime linker rtld (ld-linux)
// must see PT_DYNAMIC for DT_NEEDED, DT_INIT, DT_STRTAB, DT_SYMTAB, etc.
// This is because de-compression happens *after* linker processing.
// So everything below xct_off must appear in the output.