; File fragtest.s
;
; Tests frag handling

        ba      l1          ; 2, 254 = 0xFE
        nop
        .space  124,0
        ba      l2          ; 2, 226 = 0xE2
        nop
        .space  124,0
l1:
        .space  100,0
l2:

        ba      l3          ; 4, 256 = 0x0100
        nop
        .space  124,0
        ba      l4          ; 4, 1126 = 0x0466
        nop
        .space  124,0
l3:
        .space  1000,0
l4:

        ba      l5          ; 4, 264 = 0x0108
        nop
        .space  124,0
        ba      l6          ; 12, 33126 = 0x00008844
        nop
        .space  124,0
l5:
        .space  33000,0
l6:


; A circular case

l7:
        .space  124,0
        ba      l8          ; 2, 254 = 0xFE
        nop
        .space  126,0
        ba      l7          ; 2, -256 = 0x01
        nop
        .space  122,0
l8:

l9:
        .space  124,0
        ba      l10         ; 4, 258 = 0x0102
        nop
        .space  126,0
        ba      l9          ; 4, -260 = 0xFEFC
        nop
        .space  124,0
l10:

l11:
        .space  126,0
        ba      l12         ; 4, 256 = 0x0100
        nop
        .space  126,0
        ba      l11         ; 4, -262 = 0xFEFA
        nop
        .space  122,0
l12:
