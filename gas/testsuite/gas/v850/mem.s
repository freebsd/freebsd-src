
	.text
	.global memory
memory:
	ld.b 5[r5],r6
	ld.h 4[r5],r6
	ld.w 4[r5],r6
	sld.b 64[ep],r6
	sld.h 128[ep],r6
	sld.w 128[ep],r6
	st.b r5,5[r6]
	st.h r5,4[r6]
	st.w r5,4[r6]
	sst.b r6,64[ep]
	sst.h r6,128[ep]
	sst.w r6,128[ep]
