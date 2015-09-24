/*
 * ProFTPD - FTP server testsuite
 * Copyright (c) 2008-2015 The ProFTPD Project team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 * As a special exemption, The ProFTPD Project team and other respective
 * copyright holders give permission to link this program with OpenSSL, and
 * distribute the resulting executable, without including the source code for
 * OpenSSL in the source distribution.
 */

/* FSIO API tests */

#include "tests.h"

static pool *p = NULL;

static char *fsio_cwd = NULL;
static const char *fsio_test_path = "/tmp/prt-foo.bar.baz";
static const char *fsio_test2_path = "/tmp/prt-foo.bar.baz.quxx.quzz";
static const char *fsio_unlink_path = "/tmp/prt-fsio-link.dat";
static const char *fsio_link_path = "/tmp/prt-fsio-symlink.lnk";
static const char *fsio_testdir_path = "/tmp/prt-fsio-test.d";

/* Fixtures */

static void set_up(void) {
  if (fsio_cwd != NULL) {
    free(fsio_cwd);
  }

  fsio_cwd = getcwd(NULL, 0);

  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  init_fs();
  pr_fs_statcache_set_policy(PR_TUNABLE_FS_STATCACHE_SIZE,
    PR_TUNABLE_FS_STATCACHE_MAX_AGE, 0);

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("fsio", 1, 20);
    pr_trace_set_levels("fs.statcache", 1, 20);
  }
}

static void tear_down(void) {
  if (fsio_cwd != NULL) {
    free(fsio_cwd);
    fsio_cwd = NULL;
  }

  (void) pr_fsio_guard_chroot(FALSE);
  pr_fs_statcache_set_policy(PR_TUNABLE_FS_STATCACHE_SIZE,
    PR_TUNABLE_FS_STATCACHE_MAX_AGE, 0);

  pr_unregister_fs("/testuite");

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("fsio", 0, 0);
    pr_trace_set_levels("fs.statcache", 0, 0);
  }

  (void) unlink(fsio_test_path);
  (void) unlink(fsio_test2_path);
  (void) unlink(fsio_link_path);
  (void) unlink(fsio_unlink_path);
  (void) rmdir(fsio_testdir_path);

  if (p) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  }

}

/* Tests */

START_TEST (fsio_sys_open_test) {
  int flags;
  pr_fh_t *fh;

  mark_point();
  flags = O_CREAT|O_EXCL|O_RDONLY;
  fh = pr_fsio_open(NULL, flags);
  fail_unless(fh == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  flags = O_RDONLY;
  fh = pr_fsio_open(fsio_test_path, flags);
  fail_unless(fh == NULL, "Failed to handle non-existent file '%s'",
    fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), %s (%d)", ENOENT,
    strerror(errno), errno);

  mark_point();
  flags = O_RDONLY;
  fh = pr_fsio_open("/etc/resolv.conf", flags);
  fail_unless(fh != NULL, "Failed to /etc/resolv.conf: %s", strerror(errno));

  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_open_canon_test) {
  int flags;
  pr_fh_t *fh;

  flags = O_CREAT|O_EXCL|O_RDONLY;
  fh = pr_fsio_open_canon(NULL, flags);
  fail_unless(fh == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), %s (%d)", EINVAL,
    strerror(errno), errno);

  flags = O_RDONLY;
  fh = pr_fsio_open_canon(fsio_test_path, flags);
  fail_unless(fh == NULL, "Failed to handle non-existent file '%s'",
    fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), %s (%d)", ENOENT,
    strerror(errno), errno);

  flags = O_RDONLY;
  fh = pr_fsio_open_canon("/etc/resolv.conf", flags);
  fail_unless(fh != NULL, "Failed to /etc/resolv.conf: %s", strerror(errno));

  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_open_chroot_guard_test) {
  int flags, res;
  pr_fh_t *fh;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  flags = O_CREAT;
  fh = pr_fsio_open("/etc/resolv.conf", flags);
  if (fh != NULL) {
    (void) pr_fsio_close(fh);
    fail("open(2) of /etc/resolv.conf succeeded unexpectedly");
  }

  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);

  fh = pr_fsio_open("/etc/resolv.conf", flags);
  fail_unless(fh != NULL, "Failed to open '/etc/resolv.conf': %s",
    strerror(errno));
  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_close_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_close(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s %d", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open("/etc/resolv.conf", O_RDONLY);
  fail_unless(fh != NULL, "Failed to open /etc/resolv.conf: %s",
    strerror(errno));

  res = pr_fsio_close(fh);
  fail_unless(res == 0, "Failed to close file handle: %s", strerror(errno));

  mark_point();

  /* Deliberately try to close an already-closed handle, to make sure we
   * don't segfault.
   */
  res = pr_fsio_close(fh);
  fail_unless(res < 0, "Failed to handle already-closed file handle");
  fail_unless(errno == EBADF, "Expected EBADF (%d), got %s (%d)", EBADF,
    strerror(errno), errno);
}
END_TEST

START_TEST (fsio_sys_unlink_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_unlink(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_unlink_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to open '%s': %s", fsio_unlink_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  res = pr_fsio_unlink(fsio_unlink_path);
  fail_unless(res == 0, "Failed to unlink '%s': %s", fsio_unlink_path,
    strerror(errno));
}
END_TEST

START_TEST (fsio_sys_unlink_canon_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_unlink_canon(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_unlink_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to open '%s': %s", fsio_unlink_path, 
    strerror(errno));
  (void) pr_fsio_close(fh);

  res = pr_fsio_unlink_canon(fsio_unlink_path);
  fail_unless(res == 0, "Failed to unlink '%s': %s", fsio_unlink_path,
    strerror(errno));
}
END_TEST

START_TEST (fsio_sys_unlink_chroot_guard_test) {
  int res;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_unlink("/etc/resolv.conf");
  fail_unless(res < 0, "Deleted /etc/resolv.conf unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);

  res = pr_fsio_unlink("/lib/foo.bar.baz");
  fail_unless(res < 0, "Deleted /lib/foo.bar.baz unexpectedly");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s %d", ENOENT,
    strerror(errno), errno);
}
END_TEST

START_TEST (fsio_sys_creat_test) {
  int flags;
  pr_fh_t *fh;

  flags = O_CREAT|O_EXCL|O_RDONLY;
  fh = pr_fsio_creat(NULL, flags);
  fail_unless(fh == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), %s (%d)", EINVAL,
    strerror(errno), errno);

  flags = O_RDWR;
  fh = pr_fsio_creat(fsio_test_path, flags);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
     strerror(errno));
  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);

  flags = O_EXCL|O_RDONLY;
  fh = pr_fsio_creat("/etc/resolv.conf", flags);
  fail_unless(fh == NULL, "Created /etc/resolv.conf unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);
}
END_TEST

START_TEST (fsio_sys_creat_canon_test) {
  int flags;
  pr_fh_t *fh;

  flags = O_CREAT|O_EXCL|O_RDONLY;
  fh = pr_fsio_creat_canon(NULL, flags);
  fail_unless(fh == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), %s (%d)", EINVAL,
    strerror(errno), errno);

  flags = O_RDWR;
  fh = pr_fsio_creat_canon(fsio_test_path, flags);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);

  flags = O_EXCL|O_RDONLY;
  fh = pr_fsio_creat_canon("/etc/resolv.conf", flags);
  fail_unless(fh == NULL, "Created /etc/resolv.conf unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), %s (%d)", EACCES,
    strerror(errno), errno);
}
END_TEST

START_TEST (fsio_sys_creat_chroot_guard_test) {
  int flags, res;
  pr_fh_t *fh;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  flags = O_RDWR;
  fh = pr_fsio_creat("/etc/resolv.conf", flags);
  if (fh != NULL) {
    (void) pr_fsio_close(fh);
    fail("creat(2) of /etc/resolv.conf succeeded unexpectedly");
  }

  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);

  fh = pr_fsio_creat("/lib/foo.bar.baz.d/test.dat", flags);
  fail_unless(fh == NULL, "Created /lib/foo.bar.baz.d/test.dat unexpectedly");
}
END_TEST

START_TEST (fsio_sys_stat_test) {
  int res;
  struct stat st;
  unsigned int cache_size = 3, max_age = 1, policy_flags = 0;

  res = pr_fsio_stat(NULL, &st);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat("/", NULL);
  fail_unless(res < 0, "Failed to handle null struct stat");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat("/", &st);
  fail_unless(res == 0, "Unexpected stat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  /* Now, do the stat(2) again, and make sure we get the same information
   * from the cache.
   */
  res = pr_fsio_stat("/", &st);
  fail_unless(res == 0, "Unexpected stat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  pr_fs_statcache_reset();
  res = pr_fs_statcache_set_policy(cache_size, max_age, policy_flags);
  fail_unless(res == 0, "Failed to set statcache policy: %s", strerror(errno));

  res = pr_fsio_stat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  /* Now wait for longer than 1 second (our configured max age) */
  sleep(max_age + 1);

  res = pr_fsio_stat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);
}
END_TEST

START_TEST (fsio_sys_stat_canon_test) {
  int res;
  struct stat st;
  unsigned int cache_size = 3, max_age = 1, policy_flags = 0;

  res = pr_fsio_stat_canon(NULL, &st);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat_canon("/", NULL);
  fail_unless(res < 0, "Failed to handle null struct stat");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat_canon("/", &st);
  fail_unless(res == 0, "Unexpected stat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  /* Now, do the stat(2) again, and make sure we get the same information
   * from the cache.
   */
  res = pr_fsio_stat_canon("/", &st);
  fail_unless(res == 0, "Unexpected stat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  pr_fs_statcache_reset();
  res = pr_fs_statcache_set_policy(cache_size, max_age, policy_flags);
  fail_unless(res == 0, "Failed to set statcache policy: %s", strerror(errno));

  res = pr_fsio_stat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_stat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  /* Now wait for longer than 1 second (our configured max age) */
  sleep(max_age + 1);

  res = pr_fsio_stat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);
}
END_TEST

START_TEST (fsio_sys_fstat_test) {
  int res;
  pr_fh_t *fh;
  struct stat st;

  res = pr_fsio_fstat(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open("/etc/resolv.conf", O_RDONLY);
  fail_unless(fh != NULL, "Failed to open /etc/resolv.conf: %s",
    strerror(errno));

  res = pr_fsio_fstat(fh, &st);
  fail_unless(res == 0, "Failed to fstat /etc/resolv.conf: %s",
    strerror(errno));
  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_read_test) {
  int res;
  pr_fh_t *fh;
  char *buf;
  size_t buflen;

  res = pr_fsio_read(NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open("/etc/resolv.conf", O_RDONLY);
  fail_unless(fh != NULL, "Failed to open /etc/resolv.conf: %s",
    strerror(errno));

  res = pr_fsio_read(fh, NULL, 0);
  fail_unless(res < 0, "Failed to handle null buffer");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  buflen = 32;
  buf = palloc(p, buflen);

  res = pr_fsio_read(fh, buf, 0);
  fail_unless(res < 0, "Failed to handle zero buffer length");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_read(fh, buf, 1);
  fail_unless(res == 1, "Failed to read 1 byte: %s", strerror(errno));

  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_write_test) {
  int res;
  pr_fh_t *fh;
  char *buf;
  size_t buflen;

  res = pr_fsio_write(NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to open '%s': %s", strerror(errno));

  /* XXX What happens if we use NULL buffer, zero length? */
  res = pr_fsio_write(fh, NULL, 0);
  fail_unless(res < 0, "Failed to handle null buffer");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  buflen = 32;
  buf = palloc(p, buflen);
  memset(buf, 'c', buflen);

  res = pr_fsio_write(fh, buf, 0);
  fail_unless(res == 0, "Failed to handle zero buffer length");

  res = pr_fsio_write(fh, buf, buflen);
  fail_unless(res == buflen, "Failed to write %lu bytes: %s",
    (unsigned long) buflen, strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_lseek_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_lseek(NULL, 0, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open("/etc/resolv.conf", O_RDONLY);
  fail_unless(fh != NULL, "Failed to open /etc/resolv.conf: %s",
    strerror(errno));

  res = pr_fsio_lseek(fh, 0, 0);
  fail_unless(res == 0, "Failed to seek to byte 0: %s", strerror(errno));

  (void) pr_fsio_close(fh);
}
END_TEST

START_TEST (fsio_sys_link_test) {
  int res;
  const char *target_path, *link_path;
  pr_fh_t *fh;

  target_path = link_path = NULL;
  res = pr_fsio_link(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = fsio_test_path;
  link_path = NULL;
  res = pr_fsio_link(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null link_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = NULL;
  link_path = fsio_link_path;
  res = pr_fsio_link(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null target_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  /* Link a file (that exists) to itself */
  link_path = target_path = fsio_test_path;
  res = pr_fsio_link(target_path, link_path);
  fail_unless(res < 0, "Failed to handle same existing source/destination");
  fail_unless(errno == EEXIST, "Expected EEXIST, got %s (%d)", strerror(errno),
    errno);

  /* Create expected link */
  link_path = fsio_link_path;
  target_path = fsio_test_path;
  res = pr_fsio_link(target_path, link_path);
  fail_unless(res == 0, "Failed to create link from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));
  (void) unlink(link_path);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_link_canon_test) {
  int res;
  const char *target_path, *link_path;
  pr_fh_t *fh;

  target_path = link_path = NULL;
  res = pr_fsio_link_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = fsio_test_path;
  link_path = NULL;
  res = pr_fsio_link_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null link_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = NULL;
  link_path = fsio_link_path;
  res = pr_fsio_link_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null target_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  /* Link a file (that exists) to itself */
  link_path = target_path = fsio_test_path;
  res = pr_fsio_link_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle same existing source/destination");
  fail_unless(errno == EEXIST, "Expected EEXIST, got %s (%d)", strerror(errno),
    errno);

  /* Create expected link */
  link_path = fsio_link_path;
  target_path = fsio_test_path;
  res = pr_fsio_link_canon(target_path, link_path);
  fail_unless(res == 0, "Failed to create link from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));
  (void) unlink(link_path);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_link_chroot_guard_test) {
  int res;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_link(fsio_link_path, "/etc/foo.bar.baz");
  fail_unless(res < 0, "Linked /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_symlink_test) {
  int res;
  const char *target_path, *link_path;

  target_path = link_path = NULL;
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = "/tmp";
  link_path = NULL;
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null link_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = NULL;
  link_path = fsio_link_path;
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null target_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Symlink a file (that exists) to itself */
  link_path = target_path = "/tmp";
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res < 0, "Failed to handle same existing source/destination");
  fail_unless(errno == EEXIST, "Expected EEXIST, got %s (%d)", strerror(errno),
    errno);

  /* Create expected symlink */
  link_path = fsio_link_path;
  target_path = "/tmp";
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res == 0, "Failed to create symlink from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));
  (void) unlink(link_path);
}
END_TEST

START_TEST (fsio_sys_symlink_canon_test) {
  int res;
  const char *target_path, *link_path;

  target_path = link_path = NULL;
  res = pr_fsio_symlink_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = "/tmp";
  link_path = NULL;
  res = pr_fsio_symlink_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null link_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  target_path = NULL;
  link_path = fsio_link_path;
  res = pr_fsio_symlink_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle null target_path argument");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Symlink a file (that exists) to itself */
  link_path = target_path = "/tmp";
  res = pr_fsio_symlink_canon(target_path, link_path);
  fail_unless(res < 0, "Failed to handle same existing source/destination");
  fail_unless(errno == EEXIST, "Expected EEXIST, got %s (%d)", strerror(errno),
    errno);

  /* Create expected symlink */
  link_path = fsio_link_path;
  target_path = "/tmp";
  res = pr_fsio_symlink_canon(target_path, link_path);
  fail_unless(res == 0, "Failed to create symlink from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));
  (void) unlink(link_path);
}
END_TEST

START_TEST (fsio_sys_symlink_chroot_guard_test) {
  int res;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_symlink(fsio_link_path, "/etc/foo.bar.baz");
  fail_unless(res < 0, "Symlinked /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_readlink_test) {
  int res;
  char buf[PR_TUNABLE_BUFFER_SIZE];
  const char *link_path, *target_path, *path;

  res = pr_fsio_readlink(NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Read a non-symlink file */
  path = "/";
  res = pr_fsio_readlink(path, buf, sizeof(buf)-1);
  fail_unless(res < 0, "Failed to handle non-symlink path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Read a symlink file */
  target_path = "/tmp";
  link_path = fsio_link_path;
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res == 0, "Failed to create symlink from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));

  memset(buf, '\0', sizeof(buf));
  res = pr_fsio_readlink(link_path, buf, sizeof(buf)-1);
  fail_unless(res > 0, "Failed to read symlink '%s': %s", link_path,
    strerror(errno));
  buf[res] = '\0';
  fail_unless(strcmp(buf, target_path) == 0, "Expected '%s', got '%s'",
    target_path, buf);

  /* Read a symlink file using a zero-length buffer */
  res = pr_fsio_readlink(link_path, buf, 0);
  fail_unless(res <= 0, "Expected length <= 0, got %d", res);

  (void) unlink(link_path);
}
END_TEST

START_TEST (fsio_sys_readlink_canon_test) {
  int res;
  char buf[PR_TUNABLE_BUFFER_SIZE];
  const char *link_path, *target_path, *path;

  res = pr_fsio_readlink_canon(NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Read a non-symlink file */
  path = "/";
  res = pr_fsio_readlink_canon(path, buf, sizeof(buf)-1);
  fail_unless(res < 0, "Failed to handle non-symlink path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Read a symlink file */
  target_path = "/tmp";
  link_path = fsio_link_path;
  res = pr_fsio_symlink(target_path, link_path);
  fail_unless(res == 0, "Failed to create symlink from '%s' to '%s': %s",
    link_path, target_path, strerror(errno));

  memset(buf, '\0', sizeof(buf));
  res = pr_fsio_readlink_canon(link_path, buf, sizeof(buf)-1);
  fail_unless(res > 0, "Failed to read symlink '%s': %s", link_path,
    strerror(errno));
  buf[res] = '\0';
  fail_unless(strcmp(buf, target_path) == 0, "Expected '%s', got '%s'",
    target_path, buf);

  /* Read a symlink file using a zero-length buffer */
  res = pr_fsio_readlink_canon(link_path, buf, 0);
  fail_unless(res <= 0, "Expected length <= 0, got %d", res);

  (void) unlink(link_path);
}
END_TEST

START_TEST (fsio_sys_lstat_test) {
  int res;
  struct stat st;
  unsigned int cache_size = 3, max_age = 1, policy_flags = 0;

  res = pr_fsio_lstat(NULL, &st);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat("/", NULL);
  fail_unless(res < 0, "Failed to handle null struct stat");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat("/", &st);
  fail_unless(res == 0, "Unexpected lstat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  /* Now, do the lstat(2) again, and make sure we get the same information
   * from the cache.
   */
  res = pr_fsio_lstat("/", &st);
  fail_unless(res == 0, "Unexpected lstat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  pr_fs_statcache_reset();
  res = pr_fs_statcache_set_policy(cache_size, max_age, policy_flags);
  fail_unless(res == 0, "Failed to set statcache policy: %s", strerror(errno));

  res = pr_fsio_lstat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  /* Now wait for longer than 1 second (our configured max age) */
  sleep(max_age + 1);

  res = pr_fsio_lstat("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);
}
END_TEST

START_TEST (fsio_sys_lstat_canon_test) {
  int res;
  struct stat st;
  unsigned int cache_size = 3, max_age = 1, policy_flags = 0;

  res = pr_fsio_lstat_canon(NULL, &st);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat_canon("/", NULL);
  fail_unless(res < 0, "Failed to handle null struct stat");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat_canon("/", &st);
  fail_unless(res == 0, "Unexpected lstat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  /* Now, do the lstat(2) again, and make sure we get the same information
   * from the cache.
   */
  res = pr_fsio_lstat_canon("/", &st);
  fail_unless(res == 0, "Unexpected lstat(2) error on '/': %s",
    strerror(errno));
  fail_unless(S_ISDIR(st.st_mode), "'/' is not a directory as expected");

  pr_fs_statcache_reset();
  res = pr_fs_statcache_set_policy(cache_size, max_age, policy_flags);
  fail_unless(res == 0, "Failed to set statcache policy: %s", strerror(errno));

  res = pr_fsio_lstat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  res = pr_fsio_lstat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);

  /* Now wait for longer than 1 second (our configured max age) */
  sleep(max_age + 1);

  res = pr_fsio_lstat_canon("/foo/bar/baz/quxx", &st);
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT, got %s (%d)", strerror(errno),
    errno);
}
END_TEST

START_TEST (fsio_sys_access_dir_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  mode_t perms;

  res = pr_fsio_access(NULL, X_OK, uid, gid, NULL);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL, got %s (%d)", strerror(errno),
    errno);

  /* Make the directory to check; we want it to have perms 771.*/
  perms = (mode_t) 0771;
  res = mkdir(fsio_testdir_path, perms);
  fail_if(res < 0, "Unable to create directory '%s': %s", fsio_testdir_path,
    strerror(errno));

  /* Use chmod(2) to ensure that the directory has the perms we want,
   * regardless of any umask settings.
   */
  res = chmod(fsio_testdir_path, perms);
  fail_if(res < 0, "Unable to set perms %04o on directory '%s': %s", perms,
    fsio_testdir_path, strerror(errno));

  /* First, check that we ourselves can access our own directory. */

  pr_fs_clear_cache2(fsio_testdir_path);
  res = pr_fsio_access(fsio_testdir_path, F_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for file access on directory: %s",
    strerror(errno));

  pr_fs_clear_cache2(fsio_testdir_path);
  res = pr_fsio_access(fsio_testdir_path, R_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for read access on directory: %s",
    strerror(errno));

  pr_fs_clear_cache2(fsio_testdir_path);
  res = pr_fsio_access(fsio_testdir_path, W_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for write access on directory: %s",
    strerror(errno));

  pr_fs_clear_cache2(fsio_testdir_path);
  res = pr_fsio_access(fsio_testdir_path, X_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for execute access on directory: %s",
    strerror(errno));

  if (getenv("TRAVIS_CI") == NULL) {
    uid_t other_uid = 1000;
    gid_t other_gid = 1000;

    /* Next, check that others can access the directory. */
    pr_fs_clear_cache2(fsio_testdir_path);
    res = pr_fsio_access(fsio_testdir_path, F_OK, other_uid, other_gid,
      NULL);
    fail_unless(res == 0,
      "Failed to check for other file access on directory: %s",
      strerror(errno));

    pr_fs_clear_cache2(fsio_testdir_path);
    res = pr_fsio_access(fsio_testdir_path, R_OK, other_uid, other_gid,
      NULL);
    fail_unless(res < 0,
      "other read access on directory succeeded unexpectedly");
    fail_unless(errno == EACCES, "Expected EACCES, got %s (%d)",
      strerror(errno), errno);

    pr_fs_clear_cache2(fsio_testdir_path);
    res = pr_fsio_access(fsio_testdir_path, W_OK, other_uid, other_gid,
      NULL);
    fail_unless(res < 0,
      "other write access on directory succeeded unexpectedly");
    fail_unless(errno == EACCES, "Expected EACCES, got %s (%d)",
      strerror(errno), errno);

    pr_fs_clear_cache2(fsio_testdir_path);
    res = pr_fsio_access(fsio_testdir_path, X_OK, other_uid, other_gid,
      NULL);
    fail_unless(res == 0, "Failed to check for execute access on directory: %s",
      strerror(errno));
  }

  (void) rmdir(fsio_testdir_path);
}
END_TEST

START_TEST (fsio_sys_access_file_test) {
  int fd, res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  mode_t perms = 0664;

  /* Make the file to check; we want it to have perms 664.*/
  fd = open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_if(fd < 0, "Unable to create file '%s': %s", fsio_test_path,
    strerror(errno));

  /* Use chmod(2) to ensure that the file has the perms we want,
   * regardless of any umask settings.
   */
  res = chmod(fsio_test_path, perms);
  fail_if(res < 0, "Unable to set perms %04o on file '%s': %s", perms,
    fsio_test_path, strerror(errno));

  /* First, check that we ourselves can access our own file. */

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_access(fsio_test_path, F_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for file access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_access(fsio_test_path, R_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for read access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_access(fsio_test_path, W_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for write access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_access(fsio_test_path, X_OK, uid, gid, NULL);
  fail_unless(res < 0, "Failed to check for execute access on '%s': %s",
    fsio_test_path, strerror(errno));

  (void) unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_faccess_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  mode_t perms = 0664;
  pr_fh_t *fh;

  res = pr_fsio_faccess(NULL, F_OK, uid, gid, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Unable to create file '%s': %s", fsio_test_path,
    strerror(errno));

  /* Use chmod(2) to ensure that the file has the perms we want,
   * regardless of any umask settings.
   */
  res = chmod(fsio_test_path, perms);
  fail_if(res < 0, "Unable to set perms %04o on file '%s': %s", perms,
    fsio_test_path, strerror(errno));

  /* First, check that we ourselves can access our own file. */

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_faccess(fh, F_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for file access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_faccess(fh, R_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for read access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_faccess(fh, W_OK, uid, gid, NULL);
  fail_unless(res == 0, "Failed to check for write access on '%s': %s",
    fsio_test_path, strerror(errno));

  pr_fs_clear_cache2(fsio_test_path);
  res = pr_fsio_faccess(fh, X_OK, uid, gid, NULL);
  fail_unless(res < 0, "Failed to check for execute access on '%s': %s",
    fsio_test_path, strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_truncate_test) {
  int res;
  off_t len = 0;
  pr_fh_t *fh;

  res = pr_fsio_truncate(NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_truncate(fsio_test_path, 0);
  fail_unless(res < 0, "Truncated '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_truncate(fsio_test_path, len);
  fail_unless(res == 0, "Failed to truncate '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_truncate_canon_test) {
  int res;
  off_t len = 0;
  pr_fh_t *fh;

  res = pr_fsio_truncate_canon(NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_truncate_canon(fsio_test_path, 0);
  fail_unless(res < 0, "Truncated '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_truncate_canon(fsio_test_path, len);
  fail_unless(res == 0, "Failed to truncate '%s': %s", fsio_test_path,
    strerror(errno));
  
  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_truncate_chroot_guard_test) {
  int res;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_truncate("/etc/foo.bar.baz", 0);
  fail_unless(res < 0, "Truncated /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_ftruncate_test) {
  int res;
  off_t len = 0;
  pr_fh_t *fh;

  res = pr_fsio_ftruncate(NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_ftruncate(fh, len);
  fail_unless(res == 0, "Failed to truncate '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chmod_test) {
  int res;
  mode_t mode = 0644;
  pr_fh_t *fh;

  res = pr_fsio_chmod(NULL, mode);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chmod(fsio_test_path, 0);
  fail_unless(res < 0, "Changed perms of '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_chmod(fsio_test_path, mode);
  fail_unless(res == 0, "Failed to set perms of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chmod_canon_test) {
  int res;
  mode_t mode = 0644;
  pr_fh_t *fh;

  res = pr_fsio_chmod_canon(NULL, mode);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chmod_canon(fsio_test_path, 0);
  fail_unless(res < 0, "Changed perms of '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_chmod_canon(fsio_test_path, mode);
  fail_unless(res == 0, "Failed to set perms of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chmod_chroot_guard_test) {
  int res;
  mode_t mode = 0644;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_chmod("/etc/foo.bar.baz", mode);
  fail_unless(res < 0, "Set mode on /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_fchmod_test) {
  int res;
  mode_t mode = 0644;
  pr_fh_t *fh;

  res = pr_fsio_fchmod(NULL, mode);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_fchmod(fh, mode);
  fail_unless(res == 0, "Failed to set perms of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chown_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  pr_fh_t *fh;

  res = pr_fsio_chown(NULL, uid, gid);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chown(fsio_test_path, uid, gid);
  fail_unless(res < 0, "Changed ownership of '%s' unexpectedly",
    fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_chown(fsio_test_path, uid, gid);
  fail_unless(res == 0, "Failed to set ownership of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chown_canon_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  pr_fh_t *fh;

  res = pr_fsio_chown_canon(NULL, uid, gid);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chown_canon(fsio_test_path, uid, gid);
  fail_unless(res < 0, "Changed ownership of '%s' unexpectedly",
    fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_chown_canon(fsio_test_path, uid, gid);
  fail_unless(res == 0, "Failed to set ownership of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_chown_chroot_guard_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_chown("/etc/foo.bar.baz", uid, gid);
  fail_unless(res < 0, "Set ownership on /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_fchown_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  pr_fh_t *fh;

  res = pr_fsio_fchown(NULL, uid, gid);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_fchown(fh, uid, gid);
  fail_unless(res == 0, "Failed to set ownership of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_lchown_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();
  pr_fh_t *fh;

  res = pr_fsio_lchown(NULL, uid, gid);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_lchown(fsio_test_path, uid, gid);
  fail_unless(res < 0, "Changed ownership of '%s' unexpectedly",
    fsio_test_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  res = pr_fsio_lchown(fsio_test_path, uid, gid);
  fail_unless(res == 0, "Failed to set ownership of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_lchown_chroot_guard_test) {
  int res;
  uid_t uid = getuid();
  gid_t gid = getgid();

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_lchown("/etc/foo.bar.baz", uid, gid);
  fail_unless(res < 0, "Set ownership on /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_rename_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_rename(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_rename(fsio_test_path, NULL);
  fail_unless(res < 0, "Failed to handle null dst argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_rename(fsio_test_path, fsio_test2_path);
  fail_unless(res < 0, "Failed to handle non-existent files");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  res = pr_fsio_rename(fsio_test_path, fsio_test2_path);
  fail_unless(res == 0, "Failed to rename '%s' to '%s': %s", fsio_test_path,
    fsio_test2_path, strerror(errno));

  (void) pr_fsio_unlink(fsio_test_path);
  (void) pr_fsio_unlink(fsio_test2_path);
}
END_TEST

START_TEST (fsio_sys_rename_canon_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_rename_canon(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_rename_canon(fsio_test_path, NULL);
  fail_unless(res < 0, "Failed to handle null dst argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_rename_canon(fsio_test_path, fsio_test2_path);
  fail_unless(res < 0, "Failed to handle non-existent files");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  res = pr_fsio_rename_canon(fsio_test_path, fsio_test2_path);
  fail_unless(res == 0, "Failed to rename '%s' to '%s': %s", fsio_test_path,
    fsio_test2_path, strerror(errno));

  (void) pr_fsio_unlink(fsio_test_path);
  (void) pr_fsio_unlink(fsio_test2_path);
}
END_TEST

START_TEST (fsio_sys_rename_chroot_guard_test) {
  int res;
  pr_fh_t *fh;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));
  (void) pr_fsio_close(fh);

  res = pr_fsio_rename(fsio_test_path, "/etc/foo.bar.baz");
  fail_unless(res < 0, "Renamed '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s (%d)", EACCES,
    strerror(errno), errno);

  res = pr_fsio_rename("/etc/foo.bar.baz", fsio_test_path);
  fail_unless(res < 0, "Renamed '/etc/foo.bar.baz' unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s (%d)", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_utimes_test) {
  int res;
  struct timeval tvs[3];
  pr_fh_t *fh;

  res = pr_fsio_utimes(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_utimes(fsio_test_path, (struct timeval *) &tvs);
  fail_unless(res < 0, "Changed times of '%s' unexpectedly", fsio_test_path);
  fail_unless(errno == ENOENT || errno == EINVAL,
    "Expected ENOENT (%d) or EINVAL (%d), got %s (%d)", ENOENT, EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  memset(&tvs, 0, sizeof(tvs));
  res = pr_fsio_utimes(fsio_test_path, (struct timeval *) &tvs);
  fail_unless(res == 0, "Failed to set times of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_utimes_chroot_guard_test) {
  int res;
  struct timeval tvs[3];

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);
 
  res = pr_fsio_utimes("/etc/foo.bar.baz", (struct timeval *) &tvs);
  fail_unless(res < 0, "Set times on /etc/foo.bar.baz unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_futimes_test) {
  int res;
  struct timeval tvs[3];
  pr_fh_t *fh;
  
  res = pr_fsio_futimes(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fh = pr_fsio_open(fsio_test_path, O_CREAT|O_EXCL|O_WRONLY);
  fail_unless(fh != NULL, "Failed to create '%s': %s", fsio_test_path,
    strerror(errno));

  memset(&tvs, 0, sizeof(tvs));
  res = pr_fsio_futimes(fh, (struct timeval *) &tvs);
  fail_unless(res == 0, "Failed to set times of '%s': %s", fsio_test_path,
    strerror(errno));

  (void) pr_fsio_close(fh);
  (void) pr_fsio_unlink(fsio_test_path);
}
END_TEST

START_TEST (fsio_sys_mkdir_test) {
  int res;
  mode_t mode = 0755;

  res = pr_fsio_mkdir(NULL, mode);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_mkdir(fsio_testdir_path, mode);
  fail_unless(res == 0, "Failed to create '%s': %s", fsio_testdir_path,
    strerror(errno));

  (void) pr_fsio_rmdir(fsio_testdir_path);
}
END_TEST

START_TEST (fsio_sys_mkdir_chroot_guard_test) {
  int res;
  mode_t mode = 0755;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);
  
  res = pr_fsio_mkdir("/etc/foo.bar.baz.d", mode);
  fail_unless(res < 0, "Created /etc/foo.bar.baz.d unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_rmdir_test) {
  int res;
  mode_t mode = 0755;

  res = pr_fsio_rmdir(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_rmdir(fsio_testdir_path);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_mkdir(fsio_testdir_path, mode);
  fail_unless(res == 0, "Failed to create '%s': %s", fsio_testdir_path,
    strerror(errno));

  res = pr_fsio_rmdir(fsio_testdir_path);
  fail_unless(res == 0, "Failed to remove '%s': %s", fsio_testdir_path,
    strerror(errno));
}
END_TEST

START_TEST (fsio_sys_rmdir_chroot_guard_test) {
  int res;

  res = pr_fsio_guard_chroot(TRUE);
  fail_unless(res == FALSE, "Expected FALSE (%d), got %d", FALSE, res);

  res = pr_fsio_rmdir("/etc/foo.bar.baz.d");
  fail_unless(res < 0, "Removed /etc/foo.bar.baz.d unexpectedly");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s %d", EACCES,
    strerror(errno), errno);

  (void) pr_fsio_guard_chroot(FALSE);
}
END_TEST

START_TEST (fsio_sys_chdir_test) {
  int res;

  res = pr_fsio_chdir(NULL, FALSE);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chdir("/tmp", FALSE);
  fail_unless(res == 0, "Failed to chdir to '%s': %s", fsio_cwd,
    strerror(errno));

  res = pr_fsio_chdir(fsio_cwd, FALSE);
  fail_unless(res == 0, "Failed to chdir to '%s': %s", fsio_cwd,
    strerror(errno));
}
END_TEST

START_TEST (fsio_sys_chdir_canon_test) {
  int res;

  res = pr_fsio_chdir_canon(NULL, FALSE);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_chdir_canon("/tmp", FALSE);
  fail_unless(res == 0, "Failed to chdir to '%s': %s", fsio_cwd,
    strerror(errno));

  res = pr_fsio_chdir_canon(fsio_cwd, FALSE);
  fail_unless(res == 0, "Failed to chdir to '%s': %s", fsio_cwd,
    strerror(errno));
}
END_TEST

START_TEST (fsio_sys_chroot_test) {
  int res;

  res = pr_fsio_chroot(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  if (getuid() != 0) {
    res = pr_fsio_chroot("/tmp");
    fail_unless(res < 0, "Failed to chroot without root privs");
    fail_unless(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
      strerror(errno), errno);
  }
}
END_TEST

START_TEST (fsio_sys_opendir_test) {
  void *res;

  mark_point();
  res = pr_fsio_opendir(NULL);
  fail_unless(res == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno); 

  mark_point();
  res = pr_fsio_opendir("/etc/resolv.conf");
  fail_unless(res == NULL, "Failed to handle file argument");
  fail_unless(errno == ENOTDIR, "Expected ENOTDIR (%d), got %s (%d)", ENOTDIR,
    strerror(errno), errno);

  mark_point();
  res = pr_fsio_opendir("/tmp/");
  fail_unless(res != NULL, "Failed to open '/tmp/': %s", strerror(errno));

  (void) pr_fsio_closedir(res);
}
END_TEST

START_TEST (fsio_sys_readdir_test) {
  void *dirh;
  struct dirent *dent;

  dent = pr_fsio_readdir(NULL);
  fail_unless(dent == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  dent = pr_fsio_readdir("/etc/resolv.conf");
  fail_unless(dent == NULL, "Failed to handle file argument");
  fail_unless(errno == ENOTDIR, "Expected ENOTDIR (%d), got %s (%d)", ENOTDIR,
    strerror(errno), errno);

  mark_point();
  dirh = pr_fsio_opendir("/tmp/");
  fail_unless(dirh != NULL, "Failed to open '/tmp/': %s", strerror(errno));

  dent = pr_fsio_readdir(dirh);
  fail_unless(dent != NULL, "Failed to read directory entry: %s",
    strerror(errno));

  (void) pr_fsio_closedir(dirh);
}
END_TEST

START_TEST (fsio_sys_closedir_test) {
  void *dirh;
  int res;

  res = pr_fsio_closedir(NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  dirh = pr_fsio_opendir("/tmp/");
  fail_unless(dirh != NULL, "Failed to open '/tmp/': %s", strerror(errno));

  res = pr_fsio_closedir(dirh);
  fail_unless(res == 0, "Failed to close '/tmp/': %s", strerror(errno));

  /* Closing an already-closed directory descriptor should fail. */
  res = pr_fsio_closedir(dirh);
  fail_unless(res < 0, "Failed to handle already-closed directory handle");
  fail_unless(errno == ENOTDIR, "Expected ENOTDIR (%d), got %s (%d)", ENOTDIR,
    strerror(errno), errno);
}
END_TEST

START_TEST (fsio_statcache_clear_cache_test) {
  int expected, res;
  struct stat st;

  mark_point();
  pr_fs_clear_cache();

  res = pr_fs_clear_cache2("/testsuite");
  fail_unless(res == 0, "Failed to clear cache: %s", strerror(errno));

  res = pr_fsio_stat("/tmp", &st);
  fail_unless(res == 0, "Failed to stat '/tmp': %s", strerror(errno));

  res = pr_fs_clear_cache2("/tmp");
  expected = 1;
  fail_unless(res == expected, "Expected %d, got %d", expected, res);

  res = pr_fs_clear_cache2("/testsuite");
  expected = 0;
  fail_unless(res == expected, "Expected %d, got %d", expected, res);

  res = pr_fsio_stat("/tmp", &st);
  fail_unless(res == 0, "Failed to stat '/tmp': %s", strerror(errno));

  res = pr_fsio_lstat("/tmp", &st);
  fail_unless(res == 0, "Failed to lstat '/tmp': %s", strerror(errno));

  res = pr_fs_clear_cache2("/tmp");
  expected = 2;
  fail_unless(res == expected, "Expected %d, got %d", expected, res);
}
END_TEST

START_TEST (fsio_statcache_cache_hit_test) {
  int res;
  struct stat st;

  /* First is a cache miss...*/
  res = pr_fsio_stat("/tmp", &st);
  fail_unless(res == 0, "Failed to stat '/tmp': %s", strerror(errno));

  /* This is a cache hit, hopefully. */
  res = pr_fsio_stat("/tmp", &st);
  fail_unless(res == 0, "Failed to stat '/tmp': %s", strerror(errno));

  pr_fs_clear_cache();
}
END_TEST

START_TEST (fsio_statcache_negative_cache_test) {
  int res;
  struct stat st;

  /* First is a cache miss...*/
  res = pr_fsio_stat("/foo.bar.baz.d", &st);
  fail_unless(res < 0, "Check of '/foo.bar.baz.d' succeeded unexpectedly");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  /* This is a cache hit, hopefully. */
  res = pr_fsio_stat("/foo.bar.baz.d", &st);
  fail_unless(res < 0, "Check of '/foo.bar.baz.d' succeeded unexpectedly");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  pr_fs_clear_cache();
}
END_TEST

START_TEST (fsio_statcache_expired_test) {
  unsigned int cache_size, max_age;
  int res;
  struct stat st;

  cache_size = max_age = 1;
  pr_fs_statcache_set_policy(cache_size, max_age, 0);

  /* First is a cache miss...*/
  res = pr_fsio_stat("/tmp", &st);
  fail_unless(res == 0, "Failed to stat '/tmp': %s", strerror(errno));

  /* Wait for that cached data to expire...*/
  sleep(max_age + 1);

  /* This is another cache miss, hopefully. */
  res = pr_fsio_stat("/tmp2", &st);
  fail_unless(res < 0, "Check of '/tmp2' succeeded unexpectedly");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  pr_fs_clear_cache();
}
END_TEST

START_TEST (fsio_statcache_dump_test) {
  mark_point();
  pr_fs_statcache_dump();
}
END_TEST

START_TEST (fs_create_fs_test) {
  pr_fs_t *fs;

  fs = pr_create_fs(NULL, NULL);
  fail_unless(fs == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_create_fs(p, NULL);
  fail_unless(fs == NULL, "Failed to handle null name");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_create_fs(p, "testsuite");
  fail_unless(fs != NULL, "Failed to create FS: %s", strerror(errno));
}
END_TEST

START_TEST (fs_insert_fs_test) {
  pr_fs_t *fs;
  int res;

  res = pr_insert_fs(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_create_fs(p, "testsuite");
  fail_unless(fs != NULL, "Failed to create FS: %s", strerror(errno));

  res = pr_insert_fs(fs, NULL);
  fail_unless(res < 0, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_insert_fs(fs, "/testsuite");
  fail_unless(res == TRUE, "Failed to insert FS: %s", strerror(errno));

  res = pr_insert_fs(fs, "/testsuite");
  fail_unless(res == FALSE, "Failed to handle duplicate paths");
  fail_unless(errno == EEXIST, "Expected EEXIST (%d), got %s (%d)", EEXIST,
    strerror(errno), errno);

  (void) pr_remove_fs("/testsuite");
}
END_TEST

START_TEST (fs_get_fs_test) {
  pr_fs_t *fs, *fs2;
  int exact_match = FALSE, res;

  fs = pr_get_fs(NULL, NULL);
  fail_unless(fs == NULL, "Failed to handle null arguments");

  fs = pr_get_fs("/testsuite", &exact_match);
  fail_unless(fs != NULL, "Failed to get FS: %s", strerror(errno));
  fail_unless(exact_match == FALSE, "Expected FALSE, got TRUE");

  fs2 = pr_create_fs(p, "testsuite");
  fail_unless(fs2 != NULL, "Failed to create FS: %s", strerror(errno));

  res = pr_insert_fs(fs2, "/testsuite");
  fail_unless(res == TRUE, "Failed to insert FS: %s", strerror(errno));

  fs = pr_get_fs("/testsuite", &exact_match);
  fail_unless(fs != NULL, "Failed to get FS: %s", strerror(errno));
  fail_unless(exact_match == TRUE, "Expected TRUE, got FALSE");

  (void) pr_remove_fs("/testsuite");
}
END_TEST

START_TEST (fs_unmount_fs_test) {
  pr_fs_t *fs, *fs2;
  int res;

  fs = pr_unmount_fs(NULL, NULL);
  fail_unless(fs == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_unmount_fs("/testsuite", NULL);
  fail_unless(fs == NULL, "Failed to handle absent FS");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s (%d)", EACCES,
    strerror(errno), errno);

  fs2 = pr_create_fs(p, "testsuite");
  fail_unless(fs2 != NULL, "Failed to create FS: %s", strerror(errno));

  res = pr_insert_fs(fs2, "/testsuite");
  fail_unless(res == TRUE, "Failed to insert FS: %s", strerror(errno));

  fs = pr_unmount_fs("/testsuite", "foo bar");
  fail_unless(fs == NULL, "Failed to mismatched path AND name");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fs = pr_unmount_fs("/testsuite2", NULL);
  fail_unless(fs == NULL, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fs2 = pr_unmount_fs("/testsuite", NULL);
  fail_unless(fs2 != NULL, "Failed to unmount '/testsuite': %s",
    strerror(errno));

  (void) pr_remove_fs("/testsuite");
}
END_TEST

START_TEST (fs_remove_fs_test) {
  pr_fs_t *fs, *fs2;
  int res;

  fs = pr_remove_fs(NULL);
  fail_unless(fs == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_remove_fs("/testsuite");
  fail_unless(fs == NULL, "Failed to handle absent FS");
  fail_unless(errno == EACCES, "Expected EACCES (%d), got %s (%d)", EACCES,
    strerror(errno), errno);

  fs2 = pr_create_fs(p, "testsuite");
  fail_unless(fs2 != NULL, "Failed to create FS: %s", strerror(errno));

  res = pr_insert_fs(fs2, "/testsuite");
  fail_unless(res == TRUE, "Failed to insert FS: %s", strerror(errno));

  fs = pr_remove_fs("/testsuite2");
  fail_unless(fs == NULL, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fs2 = pr_remove_fs("/testsuite");
  fail_unless(fs2 != NULL, "Failed to remove '/testsuite': %s",
    strerror(errno));
}
END_TEST

START_TEST (fs_register_fs_test) {
  pr_fs_t *fs, *fs2;

  fs = pr_register_fs(NULL, NULL, NULL);
  fail_unless(fs == NULL, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_register_fs(p, NULL, NULL);
  fail_unless(fs == NULL, "Failed to handle null name");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_register_fs(p, "testsuite", NULL);
  fail_unless(fs == NULL, "Failed to handle null path");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  fs = pr_register_fs(p, "testsuite", "/testsuite");
  fail_unless(fs != NULL, "Failed to register FS: %s", strerror(errno));

  fs2 = pr_register_fs(p, "testsuite", "/testsuite");
  fail_unless(fs2 == NULL, "Failed to handle duplicate names");
  fail_unless(errno == EEXIST, "Expected EEXIST (%d), got %s (%d)", EEXIST,
    strerror(errno), errno);

  (void) pr_remove_fs("/testsuite");
}
END_TEST

START_TEST (fs_unregister_fs_test) {
  pr_fs_t *fs;
  int res;

  res = pr_unregister_fs(NULL);
  fail_unless(res < 0, "Failed to handle null argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_unregister_fs("/testsuite");
  fail_unless(res < 0, "Failed to handle nonexistent path");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fs = pr_register_fs(p, "testsuite", "/testsuite");
  fail_unless(fs != NULL, "Failed to register FS: %s", strerror(errno));

  res = pr_unregister_fs("/testsuite");
  fail_unless(res == 0, "Failed to unregister '/testsuite': %s",
    strerror(errno));
}
END_TEST

START_TEST (fs_resolve_fs_map_test) {
  pr_fs_t *fs;
  int res;

  mark_point();
  pr_resolve_fs_map();

  fs = pr_register_fs(p, "testsuite", "/testsuite");
  fail_unless(fs != NULL, "Failed to register FS: %s", strerror(errno));

  mark_point();
  pr_resolve_fs_map();

  res = pr_unregister_fs("/testsuite");
  fail_unless(res == 0, "Failed to unregister '/testsuite': %s",
    strerror(errno));

  mark_point();
  pr_resolve_fs_map();
}
END_TEST

#if defined(PR_USE_DEVEL)
START_TEST (fs_dump_fs_test) {
  mark_point();
  pr_fs_dump(NULL);
}
END_TEST
#endif /* PR_USE_DEVEL */

START_TEST (fs_clean_path_test) {
  char res[PR_TUNABLE_PATH_MAX+1], *path, *expected;

  res[sizeof(res)-1] = '\0';
  path = "/test.txt";
  pr_fs_clean_path(path, res, sizeof(res)-1);
  fail_unless(strcmp(res, path) == 0, "Expected cleaned path '%s', got '%s'",
    path, res);

  res[sizeof(res)-1] = '\0';
  path = "/./test.txt";
  pr_fs_clean_path(path, res, sizeof(res)-1);

  expected = "/test.txt";
  fail_unless(strcmp(res, expected) == 0,
    "Expected cleaned path '%s', got '%s'", expected, res);

  res[sizeof(res)-1] = '\0';
  path = "test.txt";
  pr_fs_clean_path(path, res, sizeof(res)-1);

  expected = "/test.txt";
  fail_unless(strcmp(res, expected) == 0,
    "Expected cleaned path '%s', got '%s'", path, res);
}
END_TEST

START_TEST (fs_clean_path2_test) {
  char res[PR_TUNABLE_PATH_MAX+1], *path, *expected;

  res[sizeof(res)-1] = '\0';
  path = "test.txt";
  pr_fs_clean_path2(path, res, sizeof(res)-1, 0);
  fail_unless(strcmp(res, path) == 0, "Expected cleaned path '%s', got '%s'",
    path, res);

  res[sizeof(res)-1] = '\0';
  path = "/./test.txt";
  pr_fs_clean_path2(path, res, sizeof(res)-1, 0);

  expected = "/test.txt";
  fail_unless(strcmp(res, expected) == 0,
    "Expected cleaned path '%s', got '%s'", expected, res);

  res[sizeof(res)-1] = '\0';
  path = "test.d///test.txt";
  pr_fs_clean_path2(path, res, sizeof(res)-1, 0);

  expected = "test.d/test.txt";
  fail_unless(strcmp(res, expected) == 0,
    "Expected cleaned path '%s', got '%s'", expected, res);

  res[sizeof(res)-1] = '\0';
  path = "/test.d///test.txt";
  pr_fs_clean_path2(path, res, sizeof(res)-1,
    PR_FSIO_CLEAN_PATH_FL_MAKE_ABS_PATH);

  expected = "/test.d/test.txt";
  fail_unless(strcmp(res, expected) == 0,
    "Expected cleaned path '%s', got '%s'", expected, res);

}
END_TEST

START_TEST (fs_dircat_test) {
  char buf[PR_TUNABLE_PATH_MAX+1], *a, *b, *ok;
  int res;

  res = pr_fs_dircat(NULL, 0, NULL, NULL);
  fail_unless(res == -1, "Failed to handle null arguments");
  fail_unless(errno == EINVAL,
    "Failed to set errno to EINVAL for null arguments");

  res = pr_fs_dircat(buf, 0, "foo", "bar");
  fail_unless(res == -1, "Failed to handle zero-length buffer");
  fail_unless(errno == EINVAL,
    "Failed to set errno to EINVAL for zero-length buffer");

  res = pr_fs_dircat(buf, -1, "foo", "bar");
  fail_unless(res == -1, "Failed to handle negative-length buffer");
  fail_unless(errno == EINVAL,
    "Failed to set errno to EINVAL for negative-length buffer");

  a = pcalloc(p, PR_TUNABLE_PATH_MAX);
  memset(a, 'A', PR_TUNABLE_PATH_MAX-1);

  b = "foo";

  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == -1, "Failed to handle too-long paths");
  fail_unless(errno == ENAMETOOLONG,
    "Failed to set errno to ENAMETOOLONG for too-long paths");

  a = "foo";
  b = "/bar";
  ok = b;
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate abs-path path second dir");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);
 
  a = "foo";
  b = "bar";
  ok = "foo/bar";
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate two normal paths");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);
 
  a = "foo/";
  b = "bar";
  ok = "foo/bar";
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate first dir with trailing slash");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);

  a = "";
  b = "";
  ok = "/";
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate two empty paths");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);

  a = "/foo";
  b = "";
  ok = "/foo/";
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate two empty paths");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);

  a = "";
  b = "/bar";
  ok = "/bar/";
  res = pr_fs_dircat(buf, sizeof(buf)-1, a, b);
  fail_unless(res == 0, "Failed to concatenate two empty paths");
  fail_unless(strcmp(buf, ok) == 0, "Expected concatenated dir '%s', got '%s'",
    ok, buf);
}
END_TEST

START_TEST (fs_setcwd_test) {
  int res;
  const char *wd;

  /* Make sure that we don't segfault if we call pr_fs_setcwd() on the
   * buffer that it is already using.
   */
  res = pr_fs_setcwd(pr_fs_getcwd());
  fail_unless(res == 0, "Failed to set cwd to '%s': %s", pr_fs_getcwd(),
    strerror(errno));

  wd = pr_fs_getcwd();
  fail_unless(wd != NULL, "Failed to get working directory: %s",
    strerror(errno));
  fail_unless(strcmp(wd, fsio_cwd) == 0,
    "Expected '%s', got '%s'", fsio_cwd, wd);

  wd = pr_fs_getvwd();
  fail_unless(wd != NULL, "Failed to get working directory: %s",
    strerror(errno));
  fail_unless(strcmp(wd, "/") == 0, "Expected '/', got '%s'", wd);
}
END_TEST

START_TEST (fs_glob_test) {
  glob_t pglob;
  int res;

  res = pr_fs_glob(NULL, 0, NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fs_glob(NULL, 0, NULL, &pglob);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  memset(&pglob, 0, sizeof(pglob));
  res = pr_fs_glob("?", 0, NULL, &pglob);
  fail_unless(res == 0, "Failed to glob: %s", strerror(errno));
  fail_unless(pglob.gl_pathc > 0, "Expected >0, got %lu",
    (unsigned long) pglob.gl_pathc);

  mark_point();
  pr_fs_globfree(NULL);
  if (res == 0) {
    pr_fs_globfree(&pglob);
  }
}
END_TEST

Suite *tests_get_fsio_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("fsio");

  testcase = tcase_create("base");
  tcase_add_checked_fixture(testcase, set_up, tear_down);

  /* Main FSIO API tests */
  tcase_add_test(testcase, fsio_sys_open_test);
  tcase_add_test(testcase, fsio_sys_open_canon_test);
  tcase_add_test(testcase, fsio_sys_open_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_close_test);
  tcase_add_test(testcase, fsio_sys_unlink_test);
  tcase_add_test(testcase, fsio_sys_unlink_canon_test);
  tcase_add_test(testcase, fsio_sys_unlink_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_creat_test);
  tcase_add_test(testcase, fsio_sys_creat_canon_test);
  tcase_add_test(testcase, fsio_sys_creat_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_stat_test);
  tcase_add_test(testcase, fsio_sys_stat_canon_test);
  tcase_add_test(testcase, fsio_sys_fstat_test);
  tcase_add_test(testcase, fsio_sys_read_test);
  tcase_add_test(testcase, fsio_sys_write_test);
  tcase_add_test(testcase, fsio_sys_lseek_test);
  tcase_add_test(testcase, fsio_sys_link_test);
  tcase_add_test(testcase, fsio_sys_link_canon_test);
  tcase_add_test(testcase, fsio_sys_link_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_symlink_test);
  tcase_add_test(testcase, fsio_sys_symlink_canon_test);
  tcase_add_test(testcase, fsio_sys_symlink_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_readlink_test);
  tcase_add_test(testcase, fsio_sys_readlink_canon_test);
  tcase_add_test(testcase, fsio_sys_lstat_test);
  tcase_add_test(testcase, fsio_sys_lstat_canon_test);
  tcase_add_test(testcase, fsio_sys_access_dir_test);
  tcase_add_test(testcase, fsio_sys_access_file_test);
  tcase_add_test(testcase, fsio_sys_faccess_test);
  tcase_add_test(testcase, fsio_sys_truncate_test);
  tcase_add_test(testcase, fsio_sys_truncate_canon_test);
  tcase_add_test(testcase, fsio_sys_truncate_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_ftruncate_test);
  tcase_add_test(testcase, fsio_sys_chmod_test);
  tcase_add_test(testcase, fsio_sys_chmod_canon_test);
  tcase_add_test(testcase, fsio_sys_chmod_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_fchmod_test);
  tcase_add_test(testcase, fsio_sys_chown_test);
  tcase_add_test(testcase, fsio_sys_chown_canon_test);
  tcase_add_test(testcase, fsio_sys_chown_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_fchown_test);
  tcase_add_test(testcase, fsio_sys_lchown_test);
  tcase_add_test(testcase, fsio_sys_lchown_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_rename_test);
  tcase_add_test(testcase, fsio_sys_rename_canon_test);
  tcase_add_test(testcase, fsio_sys_rename_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_utimes_test);
  tcase_add_test(testcase, fsio_sys_utimes_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_futimes_test);
  tcase_add_test(testcase, fsio_sys_mkdir_test);
  tcase_add_test(testcase, fsio_sys_mkdir_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_rmdir_test);
  tcase_add_test(testcase, fsio_sys_rmdir_chroot_guard_test);
  tcase_add_test(testcase, fsio_sys_chdir_test);
  tcase_add_test(testcase, fsio_sys_chdir_canon_test);
  tcase_add_test(testcase, fsio_sys_chroot_test);
  tcase_add_test(testcase, fsio_sys_opendir_test);
  tcase_add_test(testcase, fsio_sys_readdir_test);
  tcase_add_test(testcase, fsio_sys_closedir_test);

  /* FSIO statcache tests */
  tcase_add_test(testcase, fsio_statcache_clear_cache_test);
  tcase_add_test(testcase, fsio_statcache_cache_hit_test);
  tcase_add_test(testcase, fsio_statcache_negative_cache_test);
  tcase_add_test(testcase, fsio_statcache_expired_test);
  tcase_add_test(testcase, fsio_statcache_dump_test);

  /* Custom FSIO management tests */
  tcase_add_test(testcase, fs_create_fs_test);
  tcase_add_test(testcase, fs_insert_fs_test);
  tcase_add_test(testcase, fs_get_fs_test);
  tcase_add_test(testcase, fs_unmount_fs_test);
  tcase_add_test(testcase, fs_remove_fs_test);
  tcase_add_test(testcase, fs_register_fs_test);
  tcase_add_test(testcase, fs_unregister_fs_test);
  tcase_add_test(testcase, fs_resolve_fs_map_test);
#if defined(PR_USE_DEVEL)
  tcase_add_test(testcase, fs_dump_fs_test);
#endif /* PR_USE_DEVEL */

  /* Misc */
  tcase_add_test(testcase, fs_clean_path_test);
  tcase_add_test(testcase, fs_clean_path2_test);
  tcase_add_test(testcase, fs_dircat_test);
  tcase_add_test(testcase, fs_setcwd_test);
  tcase_add_test(testcase, fs_glob_test);

#if 0
  tcase_add_test(testcase, fs_copy_file_test);
  tcase_add_test(testcase, fs_interpolate_test);
  tcase_add_test(testcase, fs_resolve_partial_test);
  tcase_add_test(testcase, fs_resolve_path_test);

  tcase_add_test(testcase, fs_use_encoding_test);

  /* XXX Especially test a path that cannot be decoded */
  tcase_add_test(testcase, fs_decode_path2_test);

  /* XXX Especially test a path that cannot be encoded */
  tcase_add_test(testcase, fs_encode_path_test);

  tcase_add_test(testcase, fs_virtual_path_test);
  tcase_add_test(testcase, fs_get_usable_fd_test);
  tcase_add_test(testcase, fs_get_usable_fd2_test);
  tcase_add_test(testcase, fs_getsize_test);
  tcase_add_test(testcase, fs_getsize2_test);
  tcase_add_test(testcase, fs_fgetsize_test);
  tcase_add_test(testcase, fs_is_nfs_test);

  /* XXX With and without use_mkdtemp() */
  tcase_add_test(testcase, fsio_smkdir_test);

  tcase_add_test(testcase, fsio_getpipebuf_test);
  tcase_add_test(testcase, fsio_gets_test);

  /* XXX include the backslash-newline wrapping/handling */
  tcase_add_test(testcase, fsio_getline_test);

  tcase_add_test(testcase, fsio_puts_test);

  /* XXX include set_nonblock in this test */
  tcase_add_test(testcase, fsio_blocking_test);
#endif

  suite_add_tcase(suite, testcase);
  return suite;
}
