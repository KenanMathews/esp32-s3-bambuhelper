-- @name Mission Control
-- @color 0x07E0
-- @sdk_min 1
-- @version 2.0
-- @author BambuHelper
-- @category monitor
-- @description Animated printer status: state-appropriate animation for every print phase.

local C_BG   = 0x0000
local C_GRN  = 0x07E0
local C_CYAN = 0x07FF
local C_RED  = 0xF800
local C_ORNG = 0xFD20
local C_YEL  = 0xFFE0
local C_WHT  = 0xFFFF
local C_DIM  = 0x2104
local C_GREY = 0xC618

local CX, CY = 120, 120
local PI     = math.pi
local fi     = math.floor

-- ── Shared screen + canvas + 3 labels ────────────────────────────────────────
local scr    = ui.screen()
local canvas = ui.canvas(scr, 240, 240, 0, 0)
local lbl_a  = ui.label(scr, "", {align="top_mid",    y=14,  font=14, color=C_GREY})
local lbl_b  = ui.label(scr, "", {align="center",     y=-12, font=20, color=C_WHT})
local lbl_c  = ui.label(scr, "", {align="bottom_mid", y=-14, font=14, color=C_GREY})

-- ── Print tracking ────────────────────────────────────────────────────────────
local print_start_ms = sys.millis()
local was_printing   = false
local report_saved   = false
local buzzer_played  = false

local function fmt_eta(mins)
    if mins <= 0 then return "Done" end
    if mins < 60 then return mins .. "m" end
    return fi(mins/60) .. "h " .. (mins%60) .. "m"
end

local function fmt_elapsed()
    local s = fi((sys.millis() - print_start_ms) / 1000)
    local h = fi(s/3600); local m = fi((s%3600)/60)
    if h > 0 then return h .. "h " .. m .. "m" end
    return m .. "m " .. (s%60) .. "s"
end

-- ── Color helpers ─────────────────────────────────────────────────────────────
local function lerp_col(c1, c2, f)
    if f <= 0 then return c1 end; if f >= 1 then return c2 end
    local r1=(c1>>11)&0x1F; local g1=(c1>>5)&0x3F; local b1=c1&0x1F
    local r2=(c2>>11)&0x1F; local g2=(c2>>5)&0x3F; local b2=c2&0x1F
    return (fi(r1+(r2-r1)*f)<<11)|(fi(g1+(g2-g1)*f)<<5)|fi(b1+(b2-b1)*f)
end

local function easeout(f) return 1-(1-f)*(1-f) end

-- ═══════════════════════════════════════════════════════════════════════════════
-- IDLE — Tachometer clock
-- ═══════════════════════════════════════════════════════════════════════════════
local clk_segs = {}
for i = 0, 59 do
    local mid = math.rad(i*6 - 90)
    clk_segs[i] = {
        is5 = (i%5==0),
        x1=fi(CX+104*math.cos(mid)+.5), y1=fi(CY+104*math.sin(mid)+.5),
        x2=fi(CX+116*math.cos(mid)+.5), y2=fi(CY+116*math.sin(mid)+.5),
    }
end

local function draw_clock(_t, _dt)
    local now  = sys.time()
    local hh   = now.hour
    local mm   = now.min
    local ss   = now.sec
    local ss_f = ss
    ui.canvas_clear(canvas, C_BG)
    for i = 0, 59 do
        local s = clk_segs[i]; local on = i < ss
        local col = on and (s.is5 and C_CYAN or C_WHT) or (s.is5 and C_GREY or C_DIM)
        local w   = s.is5 and (on and 4 or 3) or (on and 3 or 2)
        ui.canvas_line(canvas, s.x1, s.y1, s.x2, s.y2, col, w)
    end
    local hr = math.rad(ss_f*6 - 90)
    local hx = fi(CX+68*math.cos(hr)+.5); local hy = fi(CY+68*math.sin(hr)+.5)
    ui.canvas_line(canvas, fi(CX-18*math.cos(hr)+.5), fi(CY-18*math.sin(hr)+.5), hx, hy, C_RED, 2)
    ui.canvas_circle(canvas, CX, CY, 4, C_WHT, 4)
    ui.canvas_circle(canvas, CX, CY, 2, C_RED, 2)
    ui.label_set(lbl_a, "IDLE");        ui.label_color(lbl_a, C_GREY)
    ui.label_set(lbl_b, string.format("%02d:%02d", hh, mm)); ui.label_color(lbl_b, C_WHT)
    ui.label_set(lbl_c, bambu.printer_name()); ui.label_color(lbl_c, C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- PREPARE / HEATING — Thermometer bars
-- ═══════════════════════════════════════════════════════════════════════════════
local TBAR_TOP=58; local TBAR_H=130; local TBAR_BOT=188; local TBAR_W=24

local function temp_color(frac)
    if frac < 0.5 then return lerp_col(0x34DF, 0xFBE0, frac*2)
    else return lerp_col(0xFBE0, C_RED, (frac-0.5)*2) end
end

local function draw_bar(cx, frac)
    local fill = math.max(2, fi(frac*TBAR_H))
    local x = cx - TBAR_W//2
    ui.canvas_rect(canvas, x, TBAR_TOP, TBAR_W, TBAR_H, C_DIM, 4)
    ui.canvas_rect(canvas, x, TBAR_BOT-fill, TBAR_W, fill, temp_color(frac), 4)
end

local function draw_thermo(_t, _dt)
    local noz=bambu.nozzle_temp(); local noz_t=bambu.nozzle_target()
    local bed=bambu.bed_temp();   local bed_t=bambu.bed_target()
    local nf = noz_t>0 and math.min(noz/noz_t,1) or 0
    local bf = bed_t>0 and math.min(bed/bed_t,1) or 0
    ui.canvas_clear(canvas, C_BG)
    draw_bar(60,  nf)
    draw_bar(180, bf)
    ui.canvas_line(canvas, 48, TBAR_BOT+2, 192, TBAR_BOT+2, C_DIM, 1)
    ui.label_set(lbl_a, "HEATING UP");  ui.label_color(lbl_a, C_ORNG)
    ui.label_set(lbl_b, string.format("%.0f° / %.0f°", noz, noz_t)); ui.label_color(lbl_b, C_ORNG)
    ui.label_set(lbl_c, string.format("Bed %.0f° / %.0f°", bed, bed_t)); ui.label_color(lbl_c, C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- BED LEVEL — Sonar sweep
-- ═══════════════════════════════════════════════════════════════════════════════
local GRID_R=88
local probes={}
for row=0,4 do for col=0,4 do
    local nx=(col/4)*2-1; local ny=(row/4)*2-1
    local d=math.sqrt(nx*nx+ny*ny); local s=(d>0.9) and (0.82/d) or 0.82
    probes[#probes+1]={x=fi(CX+nx*GRID_R*s), y=fi(CY+ny*GRID_R*s), angle=math.atan(ny*s,nx*s), hit=false}
end end
table.sort(probes, function(a,b) return a.angle<b.angle end)

local sw_angle=-PI/2; local sw_hits=0; local sw_done=false; local sw_hold=0

local function reset_sweep()
    sw_angle=-PI/2; sw_hits=0; sw_done=false; sw_hold=0
    for _,p in ipairs(probes) do p.hit=false end
end

local function draw_bedlevel(_t, dt)
    if sw_done then
        sw_hold=sw_hold+dt
        if sw_hold>2000 then reset_sweep() end
    else
        sw_angle=sw_angle+(PI*2/4000)*dt
        if sw_angle>PI then sw_angle=sw_angle-PI*2 end
        for _,p in ipairs(probes) do
            if not p.hit then
                local da=sw_angle-p.angle
                while da>PI do da=da-PI*2 end; while da<-PI do da=da+PI*2 end
                if da>=0 and da<0.25 then p.hit=true; sw_hits=sw_hits+1 end
            end
        end
        if sw_hits>=#probes then sw_done=true end
    end
    ui.canvas_clear(canvas, C_BG)
    for i=1,3 do ui.canvas_circle(canvas,CX,CY,fi(GRID_R*i/3),C_DIM,1) end
    ui.canvas_line(canvas,CX-GRID_R,CY,CX+GRID_R,CY,C_DIM,1)
    ui.canvas_line(canvas,CX,CY-GRID_R,CX,CY+GRID_R,C_DIM,1)
    if not sw_done then
        for tr=3,1,-1 do
            local ta=sw_angle-tr*0.18
            ui.canvas_line(canvas,CX,CY,fi(CX+GRID_R*math.cos(ta)),fi(CY+GRID_R*math.sin(ta)),tr==3 and 0x0322 or C_DIM,1)
        end
        local ex=fi(CX+GRID_R*math.cos(sw_angle)); local ey=fi(CY+GRID_R*math.sin(sw_angle))
        ui.canvas_line(canvas,CX,CY,ex,ey,0x03EF,2)
        ui.canvas_circle(canvas,ex,ey,3,C_CYAN,2)
    end
    for _,p in ipairs(probes) do
        local r=p.hit and 4 or 2; local c=p.hit and C_GRN or C_DIM
        ui.canvas_circle(canvas,p.x,p.y,r,c,r)
        if p.hit then ui.canvas_circle(canvas,p.x,p.y,r+3,c,1) end
    end
    ui.canvas_circle(canvas,CX,CY,3,C_CYAN,3)
    ui.label_set(lbl_a, "BED LEVELING"); ui.label_color(lbl_a, C_CYAN)
    ui.label_set(lbl_b, sw_hits.." / "..#probes); ui.label_color(lbl_b, C_WHT)
    ui.label_set(lbl_c, sw_done and "Mesh complete!" or "Scanning..."); ui.label_color(lbl_c, sw_done and C_GRN or C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- HOMING — Crosshair
-- ═══════════════════════════════════════════════════════════════════════════════
local HOM_R=90; local HOM_RM=55; local HOM_GAP=22
local hom_blink=0; local hom_ping=0

local function draw_homing(t, dt)
    hom_blink=hom_blink+dt; hom_ping=hom_ping+fi(dt*0.08)
    if hom_ping>HOM_R then hom_ping=0 end
    local sw=fi(t/6)%360
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_circle(canvas,CX,CY,HOM_R,C_CYAN,1)
    ui.canvas_circle(canvas,CX,CY,HOM_RM,C_DIM,1)
    for a=0,330,30 do
        local r=math.rad(a)
        ui.canvas_line(canvas,fi(CX+(HOM_R-4)*math.cos(r)),fi(CY+(HOM_R-4)*math.sin(r)),
            fi(CX+(HOM_R+4)*math.cos(r)),fi(CY+(HOM_R+4)*math.sin(r)),C_DIM,1)
    end
    ui.canvas_line(canvas,CX,CY-HOM_R+2,CX,CY-HOM_GAP,C_GRN,1)
    ui.canvas_line(canvas,CX,CY+HOM_GAP,CX,CY+HOM_R-2,C_GRN,1)
    ui.canvas_line(canvas,CX-HOM_R+2,CY,CX-HOM_GAP,CY,C_GRN,1)
    ui.canvas_line(canvas,CX+HOM_GAP,CY,CX+HOM_R-2,CY,C_GRN,1)
    ui.canvas_arc(canvas,CX,CY,HOM_RM-4,sw,sw+60,C_GRN,3)
    if hom_ping>0 then ui.canvas_circle(canvas,CX,CY,hom_ping,C_CYAN,1) end
    if (hom_blink//500)%2==0 then
        ui.canvas_circle(canvas,CX,CY,8,C_RED,2)
        ui.canvas_line(canvas,CX-4,CY,CX+4,CY,C_WHT,1)
        ui.canvas_line(canvas,CX,CY-4,CX,CY+4,C_WHT,1)
    else ui.canvas_circle(canvas,CX,CY,8,C_DIM,1) end
    ui.label_set(lbl_a, "HOMING"); ui.label_color(lbl_a, C_GRN)
    ui.label_set(lbl_b, ""); ui.label_set(lbl_c, bambu.printer_name()); ui.label_color(lbl_c, C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- FLOW CAL — Oscilloscope wave
-- ═══════════════════════════════════════════════════════════════════════════════
local NPTS=44; local WX1=14; local WX2=226; local WCY=120
local flow_ph=0

local function draw_flowcal(_t, dt)
    flow_ph=flow_ph+dt*0.007
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_line(canvas,WX1,WCY,WX2,WCY,C_DIM,1)
    ui.canvas_line(canvas,WX2,WCY-4,WX2,WCY+4,C_GRN,1)
    local pts={}
    for i=0,NPTS do
        local tp=i/NPTS
        local x=fi(WX1+tp*(WX2-WX1))
        local y=fi(WCY+40*math.exp(-tp*3.5)*math.sin(flow_ph-tp*8))
        pts[#pts+1]=x; pts[#pts+1]=y
    end
    ui.canvas_polyline(canvas,pts,C_CYAN,2)
    local ny=fi(WCY+40*math.exp(-3.5)*math.sin(flow_ph-8))
    ui.canvas_circle(canvas,WX2,ny,4,C_CYAN,2)
    ui.label_set(lbl_a,"FLOW CAL"); ui.label_color(lbl_a,C_CYAN)
    ui.label_set(lbl_b,"Calibrating"); ui.label_color(lbl_b,C_GREY)
    ui.label_set(lbl_c,"")
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- RUNNING — Layer stack
-- ═══════════════════════════════════════════════════════════════════════════════
local VIS=60; local LH=2; local BASE_Y=188; local run_spin=0

local function profile_hw(f)
    if f<0.15 then return 28+f/0.15*8
    elseif f<0.45 then return 36-(f-0.15)/0.30*14
    elseif f<0.65 then return 22+(f-0.45)/0.20*10
    else return 32-(f-0.65)/0.35*20 end
end

local function draw_running(_t, dt)
    run_spin=run_spin+0.4*dt/1000
    local prog=bambu.progress(); local ly=bambu.layer(); local tl=bambu.total_layers()
    local eta=bambu.remaining_mins(); local vis=fi(prog/100*VIS)
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_rect(canvas,60,BASE_Y+2,120,6,C_CYAN,2)
    for li=1,vis do
        local frac=li/VIS; local hw=fi(profile_hw(frac)+.5)
        local liy=BASE_Y-li*LH; local age=vis-li
        local col=age==0 and C_ORNG or (age<3 and 0xFC00 or (age<8 and 0xF400 or C_GRN))
        local p=fi(math.abs(math.sin(run_spin))*4+.5)
        ui.canvas_rect(canvas,CX-hw+p,liy,hw*2-p*2,LH,col,0)
        if age==0 then ui.canvas_rect(canvas,CX-hw+p,liy,hw*2-p*2,1,C_WHT,0) end
    end
    if vis>0 and vis<VIS then
        local frac=vis/VIS; local hw=fi(profile_hw(frac)+.5)
        local ny=BASE_Y-vis*LH-10
        local nx=CX+fi(math.sin(run_spin*3)*hw*0.6+.5)
        nx=math.max(CX-hw+4,math.min(CX+hw-4,nx))
        ui.canvas_rect(canvas,nx-4,ny,8,7,C_DIM,1)
        ui.canvas_line(canvas,nx-3,ny+7,nx,ny+12,C_ORNG,2)
        ui.canvas_line(canvas,nx+3,ny+7,nx,ny+12,C_ORNG,2)
        ui.canvas_circle(canvas,nx,ny+12,3,C_ORNG,3)
    end
    ui.canvas_arc(canvas,CX,CY,114,0,360,C_DIM,3)
    ui.canvas_arc(canvas,CX,CY,114,270,270+fi(prog*3.59),C_GRN,3)
    local jn=bambu.job_name()
    ui.label_set(lbl_a,jn~="" and jn or "Printing"); ui.label_color(lbl_a,C_GREY)
    ui.label_set(lbl_b,prog.."%  L"..ly); ui.label_color(lbl_b,C_WHT)
    ui.label_set(lbl_c,eta>0 and fmt_eta(eta) or fmt_elapsed()); ui.label_color(lbl_c,C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- PAUSE — Slow orbit
-- ═══════════════════════════════════════════════════════════════════════════════
local ORB_R=112; local TRAIL=18; local orb_ang=-PI/2

local function draw_pause(_t, dt)
    orb_ang=orb_ang+(PI*2/6000)*dt
    if orb_ang>PI then orb_ang=orb_ang-PI*2 end
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_circle(canvas,CX,CY,ORB_R,C_DIM,4)
    for i=TRAIL,1,-1 do
        local ta=orb_ang-i*(PI*2/60)
        local f=1-i/TRAIL
        local col=(fi(f*0x0C)<<11)|(fi(f*0x18)<<5)|fi(f*0x0C)
        local r=math.max(1,fi(f*3.5+.5))
        ui.canvas_circle(canvas,fi(CX+ORB_R*math.cos(ta)+.5),fi(CY+ORB_R*math.sin(ta)+.5),r,col,r)
    end
    local dx=fi(CX+ORB_R*math.cos(orb_ang)+.5); local dy=fi(CY+ORB_R*math.sin(orb_ang)+.5)
    ui.canvas_circle(canvas,dx,dy,7,0x4208,7)
    ui.canvas_circle(canvas,dx,dy,4,C_WHT,4)
    local by=CY-14
    ui.canvas_rect(canvas,CX-16,by,6,28,C_YEL,2)
    ui.canvas_rect(canvas,CX+10,by,6,28,C_YEL,2)
    ui.label_set(lbl_a,"PAUSED"); ui.label_color(lbl_a,C_YEL)
    ui.label_set(lbl_b,bambu.progress().."%"); ui.label_color(lbl_b,C_WHT)
    ui.label_set(lbl_c,"L"..bambu.layer().." / "..bambu.total_layers()); ui.label_color(lbl_c,C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- FAILED — Red X with shake
-- ═══════════════════════════════════════════════════════════════════════════════
local FP={600,500,400,1500}; local fp=1; local fp_t=0; local shk=0

local function reset_fail() fp=1; fp_t=0; shk=0 end

local function draw_failed(_t, dt)
    fp_t=fp_t+dt
    if fp_t>=FP[fp] then fp_t=fp_t-FP[fp]; fp=fp>=4 and 1 or fp+1; if fp==1 then reset_fail() end end
    local frac=math.min(fp_t/FP[fp],1)
    shk = fp==3 and fi(math.sin(frac*PI*7)*8*(1-frac)+.5) or 0
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_arc(canvas,CX,CY,112,0,360,C_DIM,6)
    local rf=fp==1 and easeout(frac) or 1.0
    if rf>0 then ui.canvas_arc(canvas,CX,CY,112,270,270+fi(rf*359),C_RED,6) end
    local xf=fp==2 and easeout(frac) or (fp>=3 and 1 or 0)
    if xf>0 then
        local cx2=CX+shk
        local a1=fi(math.min(xf*2,1)*38+.5); local a2=fi(math.max(xf*2-1,0)*38+.5)
        if a1>0 then
            ui.canvas_line(canvas,cx2-a1,CY-a1,cx2+a1,CY+a1,0x6000,10)
            ui.canvas_line(canvas,cx2-a1,CY-a1,cx2+a1,CY+a1,C_RED,5)
        end
        if a2>0 then
            ui.canvas_line(canvas,cx2+a2,CY-a2,cx2-a2,CY+a2,0x6000,10)
            ui.canvas_line(canvas,cx2+a2,CY-a2,cx2-a2,CY+a2,C_RED,5)
        end
        if xf>=0.5 then ui.canvas_circle(canvas,cx2,CY,5,C_WHT,5) end
    end
    ui.label_set(lbl_a,"PRINT FAILED"); ui.label_color(lbl_a,C_RED)
    ui.label_set(lbl_b,"")
    ui.label_set(lbl_c,fp==4 and "Check printer" or ""); ui.label_color(lbl_c,C_GREY)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- FINISH — Checkmark + confetti
-- ═══════════════════════════════════════════════════════════════════════════════
local S1=math.sqrt((106-88)^2+(142-122)^2)
local S2=math.sqrt((148-106)^2+(100-142)^2)
local FTOT=S1+S2
local FIN_P={500,600,800,2000}; local fin_p=1; local fin_t2=0
local fin_rings={}; local fin_conf={}
local CCOLS={C_GRN,C_CYAN,C_YEL,C_ORNG,C_WHT}

local function reset_finish()
    fin_p=1; fin_t2=0; fin_rings={}
    for i=1,12 do
        local a=(i/12)*PI*2+math.random()*0.4; local sp=55+math.random()*55
        fin_conf[i]={x=CX+math.random(-8,8),y=CY+math.random(-8,8),
            vx=math.cos(a)*sp, vy=math.sin(a)*sp-30,
            life=0.7+math.random()*0.6, col=CCOLS[math.random(#CCOLS)], size=math.random(2,4)}
    end
end
reset_finish()

local function draw_finish(_t, dt)
    fin_t2=fin_t2+dt
    if fin_t2>=FIN_P[fin_p] then
        fin_t2=fin_t2-FIN_P[fin_p]; fin_p=fin_p+1
        if fin_p==3 then
            fin_rings={{r=10,life=1},{r=18,life=0.85},{r=26,life=0.7},{r=34,life=0.55}}
            for i=1,12 do
                local a=(i/12)*PI*2+math.random()*0.4; local sp=55+math.random()*55
                fin_conf[i]={x=CX+math.random(-8,8),y=CY+math.random(-8,8),
                    vx=math.cos(a)*sp, vy=math.sin(a)*sp-30,
                    life=0.7+math.random()*0.6, col=CCOLS[math.random(#CCOLS)], size=math.random(2,4)}
            end
        end
        if fin_p>4 then reset_finish() end
    end
    local frac=math.min(fin_t2/FIN_P[math.min(fin_p,4)],1)
    local ds=dt/1000
    if fin_p>=3 then
        for i=1,#fin_rings do
            fin_rings[i].r=fin_rings[i].r+120*ds; fin_rings[i].life=fin_rings[i].life-1.8*ds
        end
        for i=1,12 do
            local p=fin_conf[i]
            p.x=p.x+p.vx*ds; p.y=p.y+p.vy*ds; p.vy=p.vy+90*ds; p.life=p.life-0.7*ds
        end
    end
    ui.canvas_clear(canvas, C_BG)
    ui.canvas_arc(canvas,CX,CY,112,0,360,C_DIM,6)
    local rf=fin_p==1 and easeout(frac) or 1.0
    if rf>0 then ui.canvas_arc(canvas,CX,CY,112,270,270+fi(rf*359),C_GRN,6) end
    if fin_p>=3 then
        for i=1,#fin_rings do
            local rng=fin_rings[i]
            if rng.life>0 and rng.r<150 then
                ui.canvas_circle(canvas,CX,CY,fi(rng.r+.5),rng.life>0.5 and C_GRN or 0x02E0,2)
            end
        end
    end
    local cf=fin_p==2 and easeout(frac) or (fin_p>=3 and 1 or 0)
    if cf>0 then
        local dist=cf*FTOT
        if dist<=S1 then
            local f=dist/S1
            ui.canvas_line(canvas,88,122,fi(88+18*f),fi(122+20*f),0x02E0,9)
            ui.canvas_line(canvas,88,122,fi(88+18*f),fi(122+20*f),C_GRN,5)
        else
            ui.canvas_line(canvas,88,122,106,142,0x02E0,9); ui.canvas_line(canvas,88,122,106,142,C_GRN,5)
            local f=(dist-S1)/S2
            ui.canvas_line(canvas,106,142,fi(106+42*f),fi(142-42*f),0x02E0,9)
            ui.canvas_line(canvas,106,142,fi(106+42*f),fi(142-42*f),C_GRN,5)
        end
    end
    if fin_p>=3 then
        for i=1,12 do
            local p=fin_conf[i]
            if p.life>0 then
                local r=math.max(1,fi(p.size*p.life+.5))
                ui.canvas_circle(canvas,fi(p.x+.5),fi(p.y+.5),r,p.col,r)
            end
        end
    end
    ui.label_set(lbl_a,"COMPLETE!"); ui.label_color(lbl_a,C_GRN)
    ui.label_set(lbl_b,"")
    ui.label_set(lbl_c,fin_p==4 and "Print finished!" or ""); ui.label_color(lbl_c,C_GRN)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- State resolver + dispatch
-- ═══════════════════════════════════════════════════════════════════════════════
local draw_fns = {
    clock    = draw_clock,
    thermo   = draw_thermo,
    bedlevel = draw_bedlevel,
    homing   = draw_homing,
    flowcal  = draw_flowcal,
    running  = draw_running,
    pause    = draw_pause,
    failed   = draw_failed,
    finish   = draw_finish,
}

local function get_anim()
    local st=bambu.state(); local sg=bambu.print_stage()
    if st=="IDLE"   then return "clock"   end
    if st=="PAUSE"  then return "pause"   end
    if st=="FAILED" then return "failed"  end
    if st=="FINISH" then return "finish"  end
    if sg==1  or sg==9  then return "bedlevel" end
    if sg==2  or sg==7  then return "thermo"   end
    if sg==13           then return "homing"   end
    if sg==8  or sg==19 then return "flowcal"  end
    if st=="PREPARE"    then return "thermo"   end
    return "running"
end

local cur_anim = ""; local anim_t = 0

sys.on_tick(function(dt)
    local printing=bambu.printing()
    if printing and not was_printing then print_start_ms=sys.millis() end
    was_printing=printing

    local anim=get_anim()
    if anim~=cur_anim then
        anim_t=0
        if anim=="bedlevel" then reset_sweep() end
        if anim=="failed"   then reset_fail()  end
        if anim=="finish"   then reset_finish() end
        if anim=="homing"   then hom_blink=0; hom_ping=0 end
        if anim=="flowcal"  then flow_ph=0 end
        if anim=="pause"    then orb_ang=-PI/2 end
        if anim=="running"  then run_spin=0 end
        if anim~="finish"   then report_saved=false; buzzer_played=false end
        cur_anim=anim
    end
    anim_t=anim_t+dt

    if anim=="finish" then
        if not report_saved then
            report_saved=true
            sys.store_set("last_report_job",     bambu.job_name())
            sys.store_set("last_report_elapsed", fmt_elapsed())
            sys.store_set("last_report_layers",  tostring(bambu.total_layers()))
        end
        if not buzzer_played then buzzer_played=true; sys.beep() end
    end

    draw_fns[anim](anim_t, dt)
end)
