/* dummy channel type (>): just a global channel type */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"
#include "supported.h"

static const char chantype_desc[] = "Secondary global channel type (>)";

static int _modinit(void);
static void _moddeinit(void);

DECLARE_MODULE_AV2(chantype_dummy, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chantype_desc);

static int
_modinit(void)
{
	CharAttrs['>'] |= CHANPFX_C;
	chantypes_update();

	return 0;
}

static void
_moddeinit(void)
{
	CharAttrs['>'] &= ~CHANPFX_C;
	chantypes_update();
}
