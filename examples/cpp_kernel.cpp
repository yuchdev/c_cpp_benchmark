// C++ RAII invariant is tied to scope
struct KFd {
  int fd;
  KFd(const char *p, int fl) : fd(open(p, fl)) {}
  ~KFd() { if (fd >= 0) close(fd); }
  explicit operator bool() const { return fd >= 0; }
};

struct KBuf {
  char *p;
  KBuf(size_t n) : p((char*)kmalloc(n, GFP_KERNEL)) {}
  ~KBuf() { kfree(p); }
  explicit operator bool() const { return p != nullptr; }
};

// Cleanup is guaranteed, cleanup order is guaranteed
int process_files(const char *a, const char *b) {
  KFd fa(a, O_RDONLY);
  if (!fa)  return -ENOENT;

  KFd fb(b, O_RDONLY);
  if (!fb)  return -ENOENT;

  KBuf buf(BUF_SIZE);
  if (!buf) return -ENOMEM;

  return do_work(fa.fd, fb.fd, buf.p);
} // buf, fb, fa destroyed here
