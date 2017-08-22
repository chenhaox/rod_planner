R = BaxterKinematics_B1();

Q = zeros(14,1);
Q(1) = -pi/4;
Q(2) = pi/2;
Q(3) = pi/6;
Q(4) = -pi/5;
Q(5) = pi/3;
Q(6) = 1;
Q(7) = -1.2;
% Q(1:7) = [1.31652 0.86554 -2.62739 -2.53096 -0.110503 1.87921 -0.779376];
% 
% Q(4+7) = -pi/2;
Q(1+7) = deg2rad(0);
Q(2+7) = deg2rad(-0);
Q(3+7) = deg2rad(-0);
Q(4+7) = deg2rad(-0);
Q(5+7) = deg2rad(-100);
Q(6+7) = deg2rad(-0);
Q(7+7) = deg2rad(-0);


T1 = R.FK(Q(1:7),1)
T2 = R.FK(Q(1:7),2)

R.print2file(Q);

% T = BaxterFK(rad2deg(Q(1:7)));
% T{8};

%%
R = BaxterKinematics_B1();

TL = [eye(3) [200;0;0]; 0 0 0 1];

q0 = rand(14,1)*2*pi - pi;
% q = GDproject(q0, TL);
q = Csolve(q0, TL);
R.print2file(q);
%%
To = [-1, 0, 0, 0;
    0, -1, 0, 0;
    0, 0, 1, 0;
    0, 0, 0, 1];
T1 = R.FK(q(1:7),1)
T2 = R.FK(q(8:14),2)