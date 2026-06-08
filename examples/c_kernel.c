// Kernel module: open two files, allocate a buffer, do work
// Audit very exit path - must clean up manually
int process_files(const char *a, const char *b) {
  int fd_a = -1, fd_b = -1;
  char *buf = nullptr;
  int ret = 0;

  fd_a = open(a, O_RDONLY);
  if (fd_a < 0) {
    ret = -ENOENT;
    goto out;          // exit 1
  }

  fd_b = open(b, O_RDONLY);
  if (fd_b < 0) {
    ret = -ENOENT;
    goto out;          // exit 2
  }

  buf = kmalloc(BUF_SIZE, GFP_KERNEL);
  if (!buf) {
    ret = -ENOMEM;
    goto out;          // exit 3
  }

  ret = do_work(fd_a, fd_b, buf);
  // fall through → exit 4

out: // manual cleanup
  if (buf)   kfree(buf);
  if (fd_b >= 0) close(fd_b);
  if (fd_a >= 0) close(fd_a);
  return ret;
}
