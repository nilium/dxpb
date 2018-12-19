#define _POSIX_C_SOURCE 200809L

#include "bgraph.c"
#include "check_main.inc"
#include <stdarg.h>

START_TEST(test_basic_bgraph)
{
	bgraph grph;
	zlist_t *list;
	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);
	ck_assert_int_eq(zhash_size(grph), ARCH_NUM_MAX - 1 + 1);
	for (zhash_t *item = zhash_first(grph); item != NULL;
					item = zhash_next(grph)) {
		ck_assert_int_eq(zhash_size(item), 0);
	}
	list = bgraph_what_next_for_arch(grph, ARCH_NOARCH);
	ck_assert_ptr_nonnull(list);
	ck_assert_int_eq(zlist_size(list), 0);
	zlist_destroy(&list);
	ck_assert_ptr_nonnull(grph);
	bgraph_destroy(&grph);
	ck_assert_ptr_null(grph);
}
END_TEST

START_TEST(test_basic_bgraph_with_pkg)
{
	int rc;
	bgraph grph;
	zlist_t *list;
	struct pkg *newpkg = bpkg_init("foo", "bar", "noarch");
	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);
	rc = bgraph_insert_pkg(grph, newpkg);
	bgraph_mark_pkg_not_in_progress(grph, "foo", "bar", ARCH_NOARCH);
	ck_assert_int_eq(rc, ERR_CODE_OK);
	list = bgraph_what_next_for_arch(grph, ARCH_NOARCH);
	ck_assert_ptr_nonnull(list);
	ck_assert_int_eq(zlist_size(list), 1);
	ck_assert_ptr_eq(newpkg, zlist_first(list));
	zlist_destroy(&list);
	bgraph_destroy(&grph);
	ck_assert_ptr_null(grph);
}
END_TEST

START_TEST(test_basic_bgraph_with_pkg_replacement)
{
	int rc;
	bgraph grph;
	zlist_t *list;
	struct pkg *newpkg = bpkg_init("foo", "bar", "noarch");
	struct pkg *reppkg = bpkg_init("foo", "baz", "noarch");

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	rc = bgraph_insert_pkg(grph, newpkg);
	ck_assert_int_eq(rc, ERR_CODE_OK);

	bgraph_mark_pkg_not_in_progress(grph, "foo", "bar", ARCH_NOARCH);

	list = bgraph_what_next_for_arch(grph, ARCH_NOARCH);
	ck_assert_ptr_nonnull(list);
	ck_assert_int_eq(zlist_size(list), 1);
	ck_assert_ptr_eq(newpkg, zlist_first(list));
	zlist_destroy(&list);

	rc = bgraph_insert_pkg(grph, reppkg);
	ck_assert_int_eq(rc, ERR_CODE_OK);
	bgraph_mark_pkg_not_in_progress(grph, "foo", "baz", ARCH_NOARCH);
	list = bgraph_what_next_for_arch(grph, ARCH_NOARCH);
	ck_assert_ptr_nonnull(list);
	ck_assert_int_eq(zlist_size(list), 1);
	ck_assert_ptr_eq(reppkg, zlist_first(list));
	zlist_destroy(&list);

	bgraph_destroy(&grph);
	ck_assert_ptr_null(grph);
}
END_TEST

START_TEST(test_basic_bgraph_with_pkg_type_replacement)
{
	int rc;
	bgraph grph;
	zlist_t *list;
	struct pkg *reppkg = bpkg_init("foo", "baz", "noarch");

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	for (enum pkg_archs i = 1; i <= ARCH_HOST; i++) {
		if (i == ARCH_HOST)
			i = ARCH_TARGET;
		rc = bgraph_insert_pkg(grph, bpkg_init("foo", "bar", pkg_archs_str[i]));
		ck_assert_int_eq(rc, ERR_CODE_OK);
		bgraph_mark_pkg_not_in_progress(grph, "foo", "bar", i);
	}

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		list = bgraph_what_next_for_arch(grph, i);
		ck_assert_ptr_nonnull(list);
		ck_assert_int_eq(zlist_size(list), i != ARCH_NOARCH);
		zlist_destroy(&list);
	}

	rc = bgraph_insert_pkg(grph, reppkg);
	ck_assert_int_eq(rc, ERR_CODE_OK);
	bgraph_mark_pkg_not_in_progress(grph, "foo", "baz", ARCH_NOARCH);
	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		list = bgraph_what_next_for_arch(grph, i);
		ck_assert_ptr_nonnull(list);
		ck_assert_int_eq(zlist_size(list), i == ARCH_NOARCH);
		ck_assert_ptr_nonnull(list);
		if (i == ARCH_NOARCH)
			ck_assert_ptr_eq(reppkg, zlist_first(list));
		zlist_destroy(&list);
	}

	bgraph_destroy(&grph);
	ck_assert_ptr_null(grph);
}
END_TEST

void
do_pkg_needs(struct pkg *pkg, bwords *host, bwords *target)
{
	if (target) {
		pkg->wneeds_cross_target = bwords_clone(*target);
		pkg->wneeds_native_target = bwords_clone(*target);
	} else {
		pkg->wneeds_cross_target = bwords_new();
		pkg->wneeds_native_target = bwords_new();
	}
	if (host) {
		pkg->wneeds_cross_host = bwords_clone(*host);
		pkg->wneeds_native_host = bwords_clone(*host);
	} else {
		pkg->wneeds_cross_host = bwords_new();
		pkg->wneeds_native_host = bwords_new();
	}
	assert(pkg->wneeds_cross_target != NULL);
	assert(pkg->wneeds_native_target != NULL);
	assert(pkg->wneeds_cross_host != NULL);
	assert(pkg->wneeds_native_host != NULL);
}

void
new_package(bgraph grph, char *name, char *ver, int noarch, int present,
		bwords *host, bwords *target)
{
	struct pkg *pkg;
	int rc;
	if (noarch) {
		pkg = bpkg_init(name, ver, pkg_archs_str[ARCH_NOARCH]);
		do_pkg_needs(pkg, host, target);
		rc = bgraph_insert_pkg(grph, pkg);
		ck_assert_int_eq(rc, ERR_CODE_OK);
		if (!present)
			bgraph_mark_pkg_not_in_progress(grph, name, ver, ARCH_NOARCH);
		else
			bgraph_mark_pkg_present(grph, name, ver, ARCH_NOARCH);
	} else {
		for (enum pkg_archs i = 1; i <= ARCH_HOST; i++) {
			if (i == ARCH_HOST)
				i = ARCH_TARGET;
			pkg = bpkg_init(name, ver, pkg_archs_str[i]);
			do_pkg_needs(pkg, host, target);
			rc = bgraph_insert_pkg(grph, pkg);
			ck_assert_int_eq(rc, ERR_CODE_OK);
			if (!present)
				bgraph_mark_pkg_not_in_progress(grph, name, ver, i);
			else
				bgraph_mark_pkg_present(grph, name, ver, i);
		}
	}
	bwords_destroy(host, 0);
	bwords_destroy(target, 0);
}

void
new_virtpackage(bgraph grph, char *name, char *ver, int noarch, int present,
		char *depname, enum pkg_archs deparch)
{
	struct pkg *pkg;
	int rc;
	if (noarch) {
		pkg = bpkg_init(name, ver, pkg_archs_str[ARCH_NOARCH]);
		pkg->depname = strdup(depname);
		pkg->deparch = deparch;
		rc = bgraph_insert_pkg(grph, pkg);
		ck_assert_int_eq(rc, ERR_CODE_OK);
		if (!present)
			bgraph_mark_pkg_not_in_progress(grph, name, ver, ARCH_NOARCH);
		else
			bgraph_mark_pkg_present(grph, name, ver, ARCH_NOARCH);
	} else {
		for (enum pkg_archs i = 1; i < ARCH_HOST; i++) {
			pkg = bpkg_init(name, ver, pkg_archs_str[i]);
			pkg->depname = strdup(depname);
			pkg->deparch = deparch;
			rc = bgraph_insert_pkg(grph, pkg);
			ck_assert_int_eq(rc, ERR_CODE_OK);
			if (!present)
				bgraph_mark_pkg_not_in_progress(grph, name, ver, i);
			else
				bgraph_mark_pkg_present(grph, name, ver, i);
		}
	}
}

int
pkgnamecmp(void *pkgin, void *name)
{
	struct pkg *pkg = pkgin;
	assert(pkg->name);
	assert(pkg->ver);
	assert(pkg->ver);
	assert(pkg->cross_needs);
	assert(pkg->needs);
	assert(pkg->needs_me);
	return strcmp(pkg->name, name);
}

void
num_pkgs_for_arch(bgraph grph, enum pkg_archs arch, int num, ...)
{
	ck_assert_ptr_nonnull(grph);
	va_list ap;
	ck_assert_int_eq(1, 1); // bookmark!
	zlist_t *list = bgraph_what_next_for_arch(grph, arch);
	ck_assert_ptr_nonnull(list);
	ck_assert_int_eq(zlist_size(list), num);
	const char *arg = NULL;
	ck_assert_ptr_null(arg);
	zlist_comparefn(list, pkgnamecmp);
	va_start(ap, num);
	for (int i = 0; i < num; i++) {
		ck_assert(i < num);
		arg = va_arg(ap, const char *);
		ck_assert_int_eq(zlist_exists(list, (void*)arg), 1);
	}
	va_end(ap);
	zlist_destroy(&list);
	ck_assert_int_eq(1, 1); // A bookmark
}

START_TEST(test_basic_bgraph_with_new_package_functions)
{
	bgraph grph;

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	new_package(grph, "foo", "bar", 0, 0, NULL, NULL);
	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		num_pkgs_for_arch(grph, i, i != ARCH_NOARCH?1:0, "foo");
	}

	new_package(grph, "foo", "baz", 1, 0, NULL, NULL);
	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		num_pkgs_for_arch(grph, i, i == ARCH_NOARCH?1:0, "foo");
	}

	bgraph_destroy(&grph);
	ck_assert_ptr_null(grph);
}
END_TEST

START_TEST(test_bgraph_with_simple_graph)
{
	bgraph grph;
	int rc;

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	bwords a = bwords_make(1, "bar");
	bwords b = bwords_make(1, "baz");
	bwords c = bwords_make(1, "foo");
	bwords d = bwords_make(2, "fol", "baz");
	new_package(grph, "los", "0.1", 0, 0, NULL, &d);
	new_package(grph, "fol", "0.1", 0, 0, NULL, &c);
	new_package(grph, "foo", "0.1", 0, 0, &a, NULL);
	new_package(grph, "bar", "0.1", 1, 0, NULL, &b);
	new_package(grph, "baz", "0.1", 0, 0, NULL, NULL);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 4, "los", "foo", "baz", "fol");
		}
	}

	ck_assert_int_eq(1, 1); // Just a bookmark

	rc = bgraph_attempt_resolution(grph);
	ck_assert_int_eq(rc, ERR_CODE_OK);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 1, "baz");
			break;
		}
}
END_TEST

START_TEST(test_bgraph_with_simple_graph_partly_available)
{
	bgraph grph;
	int rc;

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	bwords a = bwords_make(1, "bar");
	bwords b = bwords_make(1, "baz");
	bwords c = bwords_make(1, "foo");
	bwords d = bwords_make(2, "foo", "baz");
	new_package(grph, "los", "0.1", 0, 0, NULL, &d);
	new_package(grph, "fol", "0.1", 0, 0, NULL, &c);
	new_package(grph, "foo", "0.1", 0, 0, &a, NULL);
	new_package(grph, "bar", "0.1", 1, 0, NULL, &b);
	new_package(grph, "baz", "0.1", 0, 1, NULL, NULL);
	ck_assert_ptr_null(a);
	ck_assert_ptr_null(b);
	ck_assert_ptr_null(c);
	ck_assert_ptr_null(d);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 3, "los", "foo", "fol");
		}
	}

	ck_assert_int_eq(1, 1); // Just a bookmark

	rc = bgraph_attempt_resolution(grph);
	ck_assert_int_eq(rc, ERR_CODE_OK);
	rc = bgraph_attempt_resolution(grph); // Should be safe to do many times
	ck_assert_int_eq(rc, ERR_CODE_OK);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 0);
			break;
		}

	bgraph_mark_pkg_present(grph, "bar", "0.1", ARCH_NOARCH);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 0);
			break;
		default:
			num_pkgs_for_arch(grph, i, 1, "foo");
			break;
		}

	ck_assert_int_eq(1, 1); // Bookmark for the test suite
	bwords e = bwords_make(1, "bar");
	ck_assert_ptr_nonnull(e);
	new_package(grph, "foo", "0.2", 0, 0, &e, NULL);
	ck_assert_ptr_null(e);

	rc = bgraph_attempt_resolution(grph);
	ck_assert_int_eq(rc, ERR_CODE_OK);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 0);
			break;
		default:
			num_pkgs_for_arch(grph, i, 1, "foo");
			break;
		}

	for (enum pkg_archs i = 1; i < ARCH_HOST; i++) {
		rc = bgraph_mark_pkg_present(grph, "foo", "0.1", i);
		ck_assert_int_eq(rc, ERR_CODE_NO);
	}

	for (enum pkg_archs i = 1; i < ARCH_HOST; i++) {
		rc = bgraph_mark_pkg_present(grph, "foo", "0.2", i);
		ck_assert_int_eq(rc, ERR_CODE_OK);
	}

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 0);
			break;
		default:
			num_pkgs_for_arch(grph, i, 2, "fol", "los");
			break;
		}

	for (enum pkg_archs i = 1; i < ARCH_HOST; i++) {
		rc = bgraph_mark_pkg_present(grph, "fol", "0.1", i);
		ck_assert_int_eq(rc, ERR_CODE_OK);
		rc = bgraph_mark_pkg_present(grph, "los", "0.1", i);
		ck_assert_int_eq(rc, ERR_CODE_OK);
	}

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 0);
			break;
		default:
			num_pkgs_for_arch(grph, i, 0);
			break;
		}
}
END_TEST

START_TEST(test_bgraph_with_simple_graph_virtpkgs)
{
	puts("NOW IN VIRTPKGS TEST");
	bgraph grph;
	int rc;

	grph = bgraph_new();
	ck_assert_ptr_nonnull(grph);

	bwords a = bwords_make(1, "bar");
	bwords b = bwords_make(1, "baz");
	bwords c = bwords_make(1, "foo");
	bwords d = bwords_make(2, "fol", "baz");
	bwords e = bwords_make(1, "los-dev");
	new_package(grph, "los", "0.1", 0, 0, NULL, &d);
	new_package(grph, "fol", "0.1", 0, 0, NULL, &c);
	new_package(grph, "foo", "0.1", 0, 0, &a, NULL);
	new_package(grph, "bar", "0.1", 1, 0, NULL, &b);
	new_package(grph, "baz", "0.1", 0, 0, NULL, NULL);
	new_virtpackage(grph, "los-dev", "0.1", 0, 0, "los", ARCH_X86_64);
	new_package(grph, "bam", "0.1", 0, 0, &e, NULL);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++) {
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 5, "los", "foo", "baz", "fol", "bam");
		}
	}

	puts("ABOUT TO RESOLVE");
	ck_assert_int_eq(1, 1); // Just a bookmark

	rc = bgraph_attempt_resolution(grph);
	ck_assert_int_eq(rc, ERR_CODE_OK);

	for (enum pkg_archs i = 0; i < ARCH_HOST; i++)
		switch (i) {
		case ARCH_NOARCH:
			num_pkgs_for_arch(grph, i, 1, "bar");
			break;
		default:
			num_pkgs_for_arch(grph, i, 1, "baz");
			break;
		}
	puts("NOW DONE WITH VIRTPKGS TEST");
}
END_TEST

Suite * test_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("BGRAPH");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_basic_bgraph);
	tcase_add_test(tc_core, test_basic_bgraph_with_pkg);
	tcase_add_test(tc_core, test_basic_bgraph_with_pkg_replacement);
	tcase_add_test(tc_core, test_basic_bgraph_with_pkg_type_replacement);
	tcase_add_test(tc_core, test_basic_bgraph_with_new_package_functions);
	tcase_add_test(tc_core, test_bgraph_with_simple_graph);
	tcase_add_test(tc_core, test_bgraph_with_simple_graph_partly_available);
	tcase_add_test(tc_core, test_bgraph_with_simple_graph_virtpkgs);
	suite_add_tcase(s, tc_core);

	return s;
}
