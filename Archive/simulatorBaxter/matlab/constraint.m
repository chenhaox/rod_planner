% constraint 

% b = 1000*[0;0;0];
% l1 = 1000*[0.024645+0.055695-0.02; -0.25; 0.118588+0.011038];
% l2 = 1000*[0.073; 0; 0.245];
% l3 = 1000*[0.102; 0; 0];
% l4 = 1000*[0.069; 0; 0.26242-0.015];
% l5 = 1000*[0.10; 0; 0];
% l6 = 1000*[0.01; 0; 0.2707];
% l7 = 1000*[0.16; 0; 0];
% lEE = 1000*[0; 0; 0.05]+[0;0;0.1*1000]; % Extension by 0.1 to be at the middle of the gripper

% syms b l1x l1y l1z l2a l2b l3 l4a l4b l5 l6a l6b l7 lEE

b = 0; 
l1x= 0.024645e3+0.055695e3-0.02e3; l1y=-0.25e3; l1z=0.118588e3+0.011038e3;
l2a=0.073e3;
l2b=0.245e3; 
l3=0.102e3;
l4a=0.069e3;
l4b=0.26242e3-0.015e3;
l5=0.1e3;
l6a=0.01e3;
l6b=0.2707e3;
l7=0.16e3;
lEE=0.15e3;

q1 = sym('q1', [7 1]);
q2 = sym('q2', [7 1]);
q = sym('q', [7 1]);

syms r11 r12 r13 t1 r21 r22 r23 t2 r31 r32 r33 t3
T_end = [r11 r12 r13 t1; 
    r21 r22 r23 t2; 
    r31 r32 r33 t3; 
    0 0 0 1]; % Rod end post (when start in origin)


%%

Tz = @(x) [cos(x) -sin(x) 0 0; sin(x) cos(x) 0 0; 0 0 1 0; 0 0 0 1];

Ty = @(x) [cos(x) 0 sin(x) 0; 0 1 0 0; -sin(x) 0 cos(x) 0; 0 0 0 1];

Tx = @(x) [1 0 0 0; 0 cos(x) -sin(x) 0; 0 sin(x) cos(x) 0; 0 0 0 1];

Tt = @(x) [eye(3) [x(1);x(2);x(3)]; 0 0 0 1];

%% Validate

% % q = zeros(7,1);
% q1=0;q2=0;q3=0;q4=0;q5=0;q6=0;q7=0;
% Ttest=eval(T);

%% Robot 1
T01 = Tz(q(1));
T02 = T01*Tt([l2a;0;l2b])*Ty(q(2))*Tx(-pi/2);
T03 = T02*Tt([l3;0;0])*Tx(q(3)+pi/2)*Ty(pi/2);
T04 = T03*Tt([l4a;0;l4b])*Ty(q(4))*Tx(pi/2);
T05 = T04*Tt([l5;0;0])*Tx(q(5)-pi/2)*Ty(pi/2);
T06 = T05*Tt([l6a;0;l6b])*Ty(q(6)-pi/2)*Tx(pi/2);
T07 = T06*Tt([l7;0;0])*Tx(q(7)+pi/2)*Ty(pi/2);
T0EE1 = T07 * Tt([0;0;lEE]);
Tt1 = Tt([0;0;b])*Tt([l1x;l1y;l1z]); % position of shoulder pf R1 relative to center of torso (world frame)

TL = T_end; % Rod transformation
To = [-1, 0, 0, 0;
    0, -1, 0, 0;
    0, 0, 1, 0;
    0, 0, 0, 1];
T1 = Tt1*T0EE1*TL*To;

for i = 1:7
    T1 = subs(T1, ['q' num2str(i)], ['q1' num2str(i)]);
end

% P1 = T1(1:3,4);
% r1 = atan(T1(2,1)/T1(1,1));
% p1 = atan(-T1(3,1)/(T1(3,2)^2+T1(3,3)^2)^0.5);
% y1 = atan(T1(3,2)/T1(3,3));


%% Robot 2
T01 = Tz(q(1));
T02 = T01*Tt([l2a;0;l2b])*Ty(q(2))*Tx(-pi/2);
T03 = T02*Tt([l3;0;0])*Tx(q(3)+pi/2)*Ty(pi/2);
T04 = T03*Tt([l4a;0;l4b])*Ty(q(4))*Tx(pi/2);
T05 = T04*Tt([l5;0;0])*Tx(q(5)-pi/2)*Ty(pi/2);
T06 = T05*Tt([l6a;0;l6b])*Ty(q(6)-pi/2)*Tx(pi/2);
T07 = T06*Tt([l7;0;0])*Tx(q(7)+pi/2)*Ty(pi/2);
T0EE2 = T07 * Tt([0;0;lEE]);
Tt2 = Tt([0;0;b])*Tt([l1x;-l1y;l1z]); % position of shoulder of R2 relative to center of torso (world frame)

T2 = Tt2*T0EE2;%*To;

for i = 1:7
    T2 = subs(T2, ['q' num2str(i)], ['q2' num2str(i)]);
end

% P2 = T2(1:3,4);
% r2 = atan(T2(2,1)/T2(1,1));
% p2 = atan(-T2(3,1)/(T2(3,2)^2+T2(3,3)^2)^0.5);
% y2 = atan(T2(3,2)/T2(3,3));

%%
q = [q1; q2];
% C = [P1-P2; r1-r2; p1-p2; y1-y2];
C = T1(1:3,1:4)-T2(1:3,1:4);
C = C(1:end);
% C = simplify(C);
J = jacobian(C,q);
% J = simplify(J);

for i = 1:7
    J = subs(J, ['q1' num2str(i)], ['q' num2str(i)]);
    J = subs(J, ['q2' num2str(i)], ['q' num2str(i+7)]);
    C = subs(C, ['q1' num2str(i)], ['q' num2str(i)]);
    C = subs(C, ['q2' num2str(i)], ['q' num2str(i+7)]);
    q = subs(q, ['q1' num2str(i)], ['q' num2str(i)]);
    q = subs(q, ['q2' num2str(i)], ['q' num2str(i+7)]);
end

for i = 1:14
    J = subs(J, ['q' num2str(i)], ['q(' num2str(i) ')']);
    C = subs(C, ['q' num2str(i)], ['q(' num2str(i) ')']);
    q = subs(q, ['q' num2str(i)], ['q(' num2str(i) ')']);
end

%% Validate
% q = [0.5236, 0.34907, 0.69813, -1.3963, 1.5708, 0, 0.35265, 1.9905, -2.1103, 1.4868, -1.5412, 0.93609]';%ones(12,1);
% eval(C)

diary ('Jacob.m');
J
diary off