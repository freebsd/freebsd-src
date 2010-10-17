	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	fcnvff,sgl,sgl %fr5,%fr10
	fcnvff,sgl,dbl %fr5,%fr10
	fcnvff,sgl,quad %fr5,%fr10
	fcnvff,dbl,sgl %fr5,%fr10
	fcnvff,dbl,dbl %fr5,%fr10
	fcnvff,dbl,quad %fr5,%fr10
	fcnvff,quad,sgl %fr5,%fr10
	fcnvff,quad,dbl %fr5,%fr10
	fcnvff,quad,quad %fr5,%fr10
	fcnvff,sgl,sgl %fr20,%fr24
	fcnvff,sgl,dbl %fr20,%fr24
	fcnvff,sgl,quad %fr20,%fr24
	fcnvff,dbl,sgl %fr20,%fr24
	fcnvff,dbl,dbl %fr20,%fr24
	fcnvff,dbl,quad %fr20,%fr24
	fcnvff,quad,sgl %fr20,%fr24
	fcnvff,quad,dbl %fr20,%fr24
	fcnvff,quad,quad %fr20,%fr24

	fcnvxf,sgl,sgl %fr5,%fr10
	fcnvxf,sgl,dbl %fr5,%fr10
	fcnvxf,sgl,quad %fr5,%fr10
	fcnvxf,dbl,sgl %fr5,%fr10
	fcnvxf,dbl,dbl %fr5,%fr10
	fcnvxf,dbl,quad %fr5,%fr10
	fcnvxf,quad,sgl %fr5,%fr10
	fcnvxf,quad,dbl %fr5,%fr10
	fcnvxf,quad,quad %fr5,%fr10
	fcnvxf,sgl,sgl %fr20,%fr24
	fcnvxf,sgl,dbl %fr20,%fr24
	fcnvxf,sgl,quad %fr20,%fr24
	fcnvxf,dbl,sgl %fr20,%fr24
	fcnvxf,dbl,dbl %fr20,%fr24
	fcnvxf,dbl,quad %fr20,%fr24
	fcnvxf,quad,sgl %fr20,%fr24
	fcnvxf,quad,dbl %fr20,%fr24
	fcnvxf,quad,quad %fr20,%fr24

	fcnvfx,sgl,sgl %fr5,%fr10
	fcnvfx,sgl,dbl %fr5,%fr10
	fcnvfx,sgl,quad %fr5,%fr10
	fcnvfx,dbl,sgl %fr5,%fr10
	fcnvfx,dbl,dbl %fr5,%fr10
	fcnvfx,dbl,quad %fr5,%fr10
	fcnvfx,quad,sgl %fr5,%fr10
	fcnvfx,quad,dbl %fr5,%fr10
	fcnvfx,quad,quad %fr5,%fr10
	fcnvfx,sgl,sgl %fr20,%fr24
	fcnvfx,sgl,dbl %fr20,%fr24
	fcnvfx,sgl,quad %fr20,%fr24
	fcnvfx,dbl,sgl %fr20,%fr24
	fcnvfx,dbl,dbl %fr20,%fr24
	fcnvfx,dbl,quad %fr20,%fr24
	fcnvfx,quad,sgl %fr20,%fr24
	fcnvfx,quad,dbl %fr20,%fr24
	fcnvfx,quad,quad %fr20,%fr24

	fcnvfxt,sgl,sgl %fr5,%fr10
	fcnvfxt,sgl,dbl %fr5,%fr10
	fcnvfxt,sgl,quad %fr5,%fr10
	fcnvfxt,dbl,sgl %fr5,%fr10
	fcnvfxt,dbl,dbl %fr5,%fr10
	fcnvfxt,dbl,quad %fr5,%fr10
	fcnvfxt,quad,sgl %fr5,%fr10
	fcnvfxt,quad,dbl %fr5,%fr10
	fcnvfxt,quad,quad %fr5,%fr10
	fcnvfxt,sgl,sgl %fr20,%fr24
	fcnvfxt,sgl,dbl %fr20,%fr24
	fcnvfxt,sgl,quad %fr20,%fr24
	fcnvfxt,dbl,sgl %fr20,%fr24
	fcnvfxt,dbl,dbl %fr20,%fr24
	fcnvfxt,dbl,quad %fr20,%fr24
	fcnvfxt,quad,sgl %fr20,%fr24
	fcnvfxt,quad,dbl %fr20,%fr24
	fcnvfxt,quad,quad %fr20,%fr24

