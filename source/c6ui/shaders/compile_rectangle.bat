@SET FXC="C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /O3 /nologo
@SET OUT=/Fo..\..\..\resources\

%FXC% /Tvs_4_0 /Emain %OUT%rectangle_vs.bin /Vnrectangle_vs rectangle.vsh
%FXC% /Tgs_4_0 /Emain %OUT%rectangle_gs.bin /Vnrectangle_gs rectangle.gsh
%FXC% /Tps_4_0 /Emain %OUT%rectangle_ps.bin /Vnrectangle_ps rectangle.psh

%FXC% /Tgs_4_0 /Emain %OUT%sprite_gs.bin /Vnsprite_gs sprite.gsh
%FXC% /Tps_4_0 /Emain %OUT%sprite_ps.bin /Vnsprite_ps sprite.psh

pause
