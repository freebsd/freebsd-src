proc policyC_PolicyInit {slave {version 1.0}} {
}
proc policyC_PolicyCleanup {slave} {
    global l

    lappend l bye
}
