@ test floating-point constant parsing.

	.arm
	.text
	.syntax unified

        vmov.f32 q0, 0.0

	vmov.f32 q0, 2.0
        vmov.f32 q0, 4.0
        vmov.f32 q0, 8.0
        vmov.f32 q0, 16.0
        vmov.f32 q0, 0.125
        vmov.f32 q0, 0.25
        vmov.f32 q0, 0.5
        vmov.f32 q0, 1.0

        vmov.f32 q0, 2.125
        vmov.f32 q0, 4.25
        vmov.f32 q0, 8.5
        vmov.f32 q0, 17.0
        vmov.f32 q0, 0.1328125
        vmov.f32 q0, 0.265625
        vmov.f32 q0, 0.53125
        vmov.f32 q0, 1.0625
        
        vmov.f32 q0, 2.25
        vmov.f32 q0, 4.5
        vmov.f32 q0, 9.0
        vmov.f32 q0, 18.0
        vmov.f32 q0, 0.140625
        vmov.f32 q0, 0.28125
        vmov.f32 q0, 0.5625
        vmov.f32 q0, 1.125
        
        vmov.f32 q0, 2.375
        vmov.f32 q0, 4.75
        vmov.f32 q0, 9.5
        vmov.f32 q0, 19.0
        vmov.f32 q0, 0.1484375
        vmov.f32 q0, 0.296875
        vmov.f32 q0, 0.59375
        vmov.f32 q0, 1.1875
        
        vmov.f32 q0, 2.5
        vmov.f32 q0, 5.0
        vmov.f32 q0, 10.0
        vmov.f32 q0, 20.0
        vmov.f32 q0, 0.15625
        vmov.f32 q0, 0.3125
        vmov.f32 q0, 0.625
        vmov.f32 q0, 1.25
        
        vmov.f32 q0, 2.625
        vmov.f32 q0, 5.25
        vmov.f32 q0, 10.5
        vmov.f32 q0, 21.0
        vmov.f32 q0, 0.1640625
        vmov.f32 q0, 0.328125
        vmov.f32 q0, 0.65625
        vmov.f32 q0, 1.3125
        
        vmov.f32 q0, 2.75
        vmov.f32 q0, 5.5
        vmov.f32 q0, 11.0
        vmov.f32 q0, 22.0
        vmov.f32 q0, 0.171875
        vmov.f32 q0, 0.34375
        vmov.f32 q0, 0.6875
        vmov.f32 q0, 1.375
        
        vmov.f32 q0, 2.875
        vmov.f32 q0, 5.75
        vmov.f32 q0, 11.5
        vmov.f32 q0, 23.0
        vmov.f32 q0, 0.1796875
        vmov.f32 q0, 0.359375
        vmov.f32 q0, 0.71875
        vmov.f32 q0, 1.4375
        
        vmov.f32 q0, 3.0
        vmov.f32 q0, 6.0
        vmov.f32 q0, 12.0
        vmov.f32 q0, 24.0
        vmov.f32 q0, 0.1875
        vmov.f32 q0, 0.375
        vmov.f32 q0, 0.75
        vmov.f32 q0, 1.5
        
        vmov.f32 q0, 3.125
        vmov.f32 q0, 6.25
        vmov.f32 q0, 12.5
        vmov.f32 q0, 25.0
        vmov.f32 q0, 0.1953125
        vmov.f32 q0, 0.390625
        vmov.f32 q0, 0.78125
        vmov.f32 q0, 1.5625
        
        vmov.f32 q0, 3.25
        vmov.f32 q0, 6.5
        vmov.f32 q0, 13.0
        vmov.f32 q0, 26.0
        vmov.f32 q0, 0.203125
        vmov.f32 q0, 0.40625
        vmov.f32 q0, 0.8125
        vmov.f32 q0, 1.625
        
        vmov.f32 q0, 3.375
        vmov.f32 q0, 6.75
        vmov.f32 q0, 13.5
        vmov.f32 q0, 27.0
        vmov.f32 q0, 0.2109375
        vmov.f32 q0, 0.421875
        vmov.f32 q0, 0.84375
        vmov.f32 q0, 1.6875

        vmov.f32 q0, 3.5
        vmov.f32 q0, 7.0
        vmov.f32 q0, 14.0
        vmov.f32 q0, 28.0
        vmov.f32 q0, 0.21875
        vmov.f32 q0, 0.4375
        vmov.f32 q0, 0.875
        vmov.f32 q0, 1.75
        
        vmov.f32 q0, 3.625
        vmov.f32 q0, 7.25
        vmov.f32 q0, 14.5
        vmov.f32 q0, 29.0
        vmov.f32 q0, 0.2265625
        vmov.f32 q0, 0.453125
        vmov.f32 q0, 0.90625
        vmov.f32 q0, 1.8125
        
        vmov.f32 q0, 3.75
        vmov.f32 q0, 7.5
        vmov.f32 q0, 15.0
        vmov.f32 q0, 30.0
        vmov.f32 q0, 0.234375
        vmov.f32 q0, 0.46875
        vmov.f32 q0, 0.9375
        vmov.f32 q0, 1.875
        
        vmov.f32 q0, 3.875
        vmov.f32 q0, 7.75
        vmov.f32 q0, 15.5
        vmov.f32 q0, 31.0
        vmov.f32 q0, 0.2421875
        vmov.f32 q0, 0.484375
        vmov.f32 q0, 0.96875
        vmov.f32 q0, 1.9375

        vmov.f32 q0, -0.0

	vmov.f32 q0, -2.0
        vmov.f32 q0, -4.0
        vmov.f32 q0, -8.0
        vmov.f32 q0, -16.0
        vmov.f32 q0, -0.125
        vmov.f32 q0, -0.25
        vmov.f32 q0, -0.5
        vmov.f32 q0, -1.0

        vmov.f32 q0, -2.125
        vmov.f32 q0, -4.25
        vmov.f32 q0, -8.5
        vmov.f32 q0, -17.0
        vmov.f32 q0, -0.1328125
        vmov.f32 q0, -0.265625
        vmov.f32 q0, -0.53125
        vmov.f32 q0, -1.0625
        
        vmov.f32 q0, -2.25
        vmov.f32 q0, -4.5
        vmov.f32 q0, -9.0
        vmov.f32 q0, -18.0
        vmov.f32 q0, -0.140625
        vmov.f32 q0, -0.28125
        vmov.f32 q0, -0.5625
        vmov.f32 q0, -1.125
        
        vmov.f32 q0, -2.375
        vmov.f32 q0, -4.75
        vmov.f32 q0, -9.5
        vmov.f32 q0, -19.0
        vmov.f32 q0, -0.1484375
        vmov.f32 q0, -0.296875
        vmov.f32 q0, -0.59375
        vmov.f32 q0, -1.1875
        
        vmov.f32 q0, -2.5
        vmov.f32 q0, -5.0
        vmov.f32 q0, -10.0
        vmov.f32 q0, -20.0
        vmov.f32 q0, -0.15625
        vmov.f32 q0, -0.3125
        vmov.f32 q0, -0.625
        vmov.f32 q0, -1.25
        
        vmov.f32 q0, -2.625
        vmov.f32 q0, -5.25
        vmov.f32 q0, -10.5
        vmov.f32 q0, -21.0
        vmov.f32 q0, -0.1640625
        vmov.f32 q0, -0.328125
        vmov.f32 q0, -0.65625
        vmov.f32 q0, -1.3125
        
        vmov.f32 q0, -2.75
        vmov.f32 q0, -5.5
        vmov.f32 q0, -11.0
        vmov.f32 q0, -22.0
        vmov.f32 q0, -0.171875
        vmov.f32 q0, -0.34375
        vmov.f32 q0, -0.6875
        vmov.f32 q0, -1.375
        
        vmov.f32 q0, -2.875
        vmov.f32 q0, -5.75
        vmov.f32 q0, -11.5
        vmov.f32 q0, -23.0
        vmov.f32 q0, -0.1796875
        vmov.f32 q0, -0.359375
        vmov.f32 q0, -0.71875
        vmov.f32 q0, -1.4375
        
        vmov.f32 q0, -3.0
        vmov.f32 q0, -6.0
        vmov.f32 q0, -12.0
        vmov.f32 q0, -24.0
        vmov.f32 q0, -0.1875
        vmov.f32 q0, -0.375
        vmov.f32 q0, -0.75
        vmov.f32 q0, -1.5
        
        vmov.f32 q0, -3.125
        vmov.f32 q0, -6.25
        vmov.f32 q0, -12.5
        vmov.f32 q0, -25.0
        vmov.f32 q0, -0.1953125
        vmov.f32 q0, -0.390625
        vmov.f32 q0, -0.78125
        vmov.f32 q0, -1.5625
        
        vmov.f32 q0, -3.25
        vmov.f32 q0, -6.5
        vmov.f32 q0, -13.0
        vmov.f32 q0, -26.0
        vmov.f32 q0, -0.203125
        vmov.f32 q0, -0.40625
        vmov.f32 q0, -0.8125
        vmov.f32 q0, -1.625
        
        vmov.f32 q0, -3.375
        vmov.f32 q0, -6.75
        vmov.f32 q0, -13.5
        vmov.f32 q0, -27.0
        vmov.f32 q0, -0.2109375
        vmov.f32 q0, -0.421875
        vmov.f32 q0, -0.84375
        vmov.f32 q0, -1.6875

        vmov.f32 q0, -3.5
        vmov.f32 q0, -7.0
        vmov.f32 q0, -14.0
        vmov.f32 q0, -28.0
        vmov.f32 q0, -0.21875
        vmov.f32 q0, -0.4375
        vmov.f32 q0, -0.875
        vmov.f32 q0, -1.75
        
        vmov.f32 q0, -3.625
        vmov.f32 q0, -7.25
        vmov.f32 q0, -14.5
        vmov.f32 q0, -29.0
        vmov.f32 q0, -0.2265625
        vmov.f32 q0, -0.453125
        vmov.f32 q0, -0.90625
        vmov.f32 q0, -1.8125
        
        vmov.f32 q0, -3.75
        vmov.f32 q0, -7.5
        vmov.f32 q0, -15.0
        vmov.f32 q0, -30.0
        vmov.f32 q0, -0.234375
        vmov.f32 q0, -0.46875
        vmov.f32 q0, -0.9375
        vmov.f32 q0, -1.875
        
        vmov.f32 q0, -3.875
        vmov.f32 q0, -7.75
        vmov.f32 q0, -15.5
        vmov.f32 q0, -31.0
        vmov.f32 q0, -0.2421875
        vmov.f32 q0, -0.484375
        vmov.f32 q0, -0.96875
        vmov.f32 q0, -1.9375
