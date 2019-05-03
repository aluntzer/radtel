#ifndef FUCK_YOU_UBUNTU
#define FUCK_YOU_UBUNTU

#include <glib.h>



typedef gint grefcount;

static void
(g_ref_count_init) (grefcount *rc)
{
  g_return_if_fail (rc != NULL);

  /* Non-atomic refcounting is implemented using the negative range
   * of signed integers:
   *
   * G_MININT                 Z¯< 0 > Z⁺                G_MAXINT
   * |----------------------------|----------------------------|
   *
   * Acquiring a reference moves us towards MININT, and releasing a
   * reference moves us towards 0.
   */
  *rc = -1;
}

/**
 * g_ref_count_inc:
 * @rc: the address of a reference count variable
 *
 * Increases the reference count.
 *
 * Since: 2.58
 */
static void
(g_ref_count_inc) (grefcount *rc)
{
  grefcount rrc;

  g_return_if_fail (rc != NULL);

  rrc = *rc;

  g_return_if_fail (rrc < 0);

  /* Check for saturation */
  if (rrc == G_MININT)
    {
      g_critical ("Reference count %p has reached saturation", rc);
      return;
    }

  rrc -= 1;

  *rc = rrc;
}

/**
 * g_ref_count_dec:
 * @rc: the address of a reference count variable
 *
 * Decreases the reference count.
 *
 * Returns: %TRUE if the reference count reached 0, and %FALSE otherwise
 *
 * Since: 2.58
 */
static gboolean
(g_ref_count_dec) (grefcount *rc)
{
  grefcount rrc;

  g_return_val_if_fail (rc != NULL, FALSE);

  rrc = *rc;

  g_return_val_if_fail (rrc < 0, FALSE);

  rrc += 1;
  if (rrc == 0)
    return TRUE;

  *rc = rrc;

  return FALSE;
}



#endif /* FUCK_YOU_UBUNTU */
