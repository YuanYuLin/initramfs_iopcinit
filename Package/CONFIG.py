import ops
import iopc

pkg_path = ""
output_dir = ""

def set_global(args):
    global pkg_path
    global output_dir
    pkg_path = args["pkg_path"]
    output_dir = args["output_path"]

def MAIN_ENV(args):
    set_global(args)

    return False

def MAIN_EXTRACT(args):
    set_global(args)

    ops.copyto(ops.path_join(pkg_path, "Makefile"), output_dir)
    ops.copyto(ops.path_join(pkg_path, "Makefile.include.app"), output_dir)
    ops.copyto(ops.path_join(pkg_path, "init.c"), output_dir)

    return True

def MAIN_PATCH(args, patch_group_name):
    set_global(args)
    for patch in iopc.get_patch_list(pkg_path, patch_group_name):
        if iopc.apply_patch(tarball_dir, patch):
            continue
        else:
            sys.exit(1)

    return True

def MAIN_CONFIGURE(args):
    set_global(args)

    return True

def MAIN_BUILD(args):
    set_global(args)

    iopc.make(output_dir)

    return False

def MAIN_INSTALL(args):
    set_global(args)

    #CMD = ["echo", "init", "|", "cpio", "-o", "-H", "newc", "|", "gzip", ">", "initramfs.cpio.gz"]
    #ops.execCmd(CMD, output_dir, False, None)
    iopc.installBin(args["pkg_name"], ops.path_join(output_dir, "iopcinit"), "")

    return False

def MAIN_SDKENV(args):
    set_global(args)

    return False

def MAIN_CLEAN_BUILD(args):
    set_global(args)
    return False

def MAIN(args):
    set_global(args)
    print "initramfs_cpio_gz"

