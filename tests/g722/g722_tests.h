/* g722_tests.h */
#ifndef G722_TESTS_H
#define G722_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Run all ITU‑T G.722 compliance tests.
 * \return 0 on success, negative error code on failure.
 */
int itu_compliance_tests(void);

#ifdef __cplusplus
}
#endif

#endif
