# include	"trek.h"

/*
**  BOARD A KLINGON
**
**	A Klingon battle cruiser is boarded.  If the boarding party
**	is successful, they take over the vessel, otherwise, you
**	have wasted a move.  Needless to say, this move is not free.
**
**	User parameters are the Klingon to be boarded and the size of
**	the boarding party.
**
**	Three things are computed.  The first is the probability that
**	the party takes over the Klingon.  This is dependent on the
**	size of the party, the condition of the Klingon (for which
**	the energy left is used, which is definately incorrect), and
**	the number of losses that the boarding party sustains.  If too
**	many of the boarding party are killed, the probability drops
**	to zero.  The second quantity computed is the losses that the
**	boarding party sustains.  This counts in your score.  It
**	depends on the absolute and relative size of the boarding
**	party and the strength of the Klingon.  The third quantity
**	computed is the number of Klingon captives you get to take.
**	It is actually computed as the number of losses they sustain
**	subtracted from the size of their crew.  It depends on the
**	relative size of the party.  All of these quantities are
**	randomized in some fashion.
*/

board()
{
	int			prob;
	int			losses;
	int			captives;
	float			t;
	int			party;

	if (checkout(XPORTER))
		return;

	k = selectklingon();
	if (!k->srndreq)
	{
		return (printf("But captain! You must request surrender first\n"));
	}

	t = party / Param.crew;

	prob = 1000 * t;
	prob =- 500 * k->power / Param.klingpwr;

	losses = party * k->power * t * 0.5 / Param.klingpwr * (franf() + 1.0);
	if (losses * 4 > party)
		prob = 0;

	captives = %%% * (1.0 - t) * 0.5 * (franf() + 1.0);

	if (prob > ranf(1000))
		success!!!;
