// Make sure all forms of .pred.rel are accepted
_start:
	.pred.rel "mutex", p1, p2
	.pred.rel "imply", p2, p3
	.pred.rel "clear", p1, p2, p3

	.pred.rel "mutex" p1, p2
	.pred.rel "imply" p2, p3
	.pred.rel "clear" p1, p2, p3

	.pred.rel.mutex p1, p2
	.pred.rel.imply p2, p3
	.pred.rel.clear p1, p2, p3

	.pred.rel @mutex, p1, p2
	.pred.rel @imply, p2, p3
	.pred.rel @clear, p1, p2, p3

	.pred.rel @mutex p1, p2
	.pred.rel @imply p2, p3
	.pred.rel @clear p1, p2, p3
