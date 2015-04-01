--
-- Copyright (c) 2015 Pedro Souza <pedrosouza@freebsd.org>
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--
-- $FreeBSD$
--

core = {};

function core.setVerbose(b)
    if (b == nil) then
        b = not core.verbose;
    end

    if (b == true) then
        loader.perform("set boot_verbose=YES");
    else
        loader.perform("unset boot_verbose");
    end
    core.verbose = b;
end

function core.setSingleUser(b)
    if (b == nil) then
        b = not core.su;
    end

    if (b == true) then
        loader.perform("set boot_single=YES");
    else
        loader.perform("unset boot_single");
    end
    core.su = b;
end

function core.setACPI(b)
    if (b == nil) then
        b = not core.acpi;
    end
    
    if (b == true) then
        loader.perform("set acpi_load=YES");
        loader.perform("set hint.acpi.0.disabled=0");
        loader.perform("unset loader.acpi_disabled_by_user");
    else
        loader.perform("unset acpi_load");
        loader.perform("set hint.acpi.0.disabled=1");
        loader.perform("set loader.acpi_disabled_by_user=1");
    end
    core.acpi = b;
end

function core.setSafeMode(b)
    if (b == nil) then
        b = not core.sm;
    end
    if (b == true) then
        loader.perform("set kern.smp.disabled=1");
        loader.perform("set hw.ata.ata_dma=0");
        loader.perform("set hw.ata.atapi_dma=0");
        loader.perform("set hw.ata.wc=0");
        loader.perform("set hw.eisa_slots=0");
        loader.perform("set kern.eventtimer.periodic=1");
        loader.perform("set kern.geom.part.check_integrity=0");
    else
        loader.perform("unset kern.smp.disabled");
        loader.perform("unset hw.ata.ata_dma");
        loader.perform("unset hw.ata.atapi_dma");
        loader.perform("unset hw.ata.wc");
        loader.perform("unset hw.eisa_slots");
        loader.perform("unset kern.eventtimer.periodic");
        loader.perform("unset kern.geom.part.check_integrity");
    end
    core.sm = b;
end

function core.kernelList()
    local k = loader.getenv("kernel");
    local v = loader.getenv("kernels") or "";
    
    local kernels = {};
    local i = 0;
    if k ~= nil then
        i = i + 1;
        kernels[i] = k;
    end
    
    for n in v:gmatch("([^; ]+)[; ]?") do
        if n ~= k then
            i = i + 1;
            kernels[i] = n;
        end
    end
    return kernels;
end

function core.setDefaults()
    core.setACPI(true);
    core.setSafeMode(false);
    core.setSingleUser(false);
    core.setVerbose(false);
end

function core.autoboot()
    loader.perform("autoboot");
end

function core.boot()
    loader.perform("boot");
end

function core.bootserial()
    local c = loader.getenv("console");

    if c ~= nil then
        if c:find("comconsole") ~= nil then
            return true;
        end
    end
    
    local s = loader.getenv("boot_serial");
    if s ~= nil then
        return true;
    end

    local m = loader.getenv("boot_multicons");
    if m ~= nil then
        return true;
    end
    return false;
end
