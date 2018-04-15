F = fopen('out3.bin', 'r');
I = fread(F, 'single');

myhdr = zeros(600,903,3,'single');

for i=0:1:600-1
    for j=0:1:903-1
        x = I((i * 4 * 903 + j * 4 + 0) + 1);
        y = I((i * 4 * 903 + j * 4 + 1) + 1);
        z = I((i * 4 * 903 + j * 4 + 2) + 1);
        
        myhdr(i + 1, j + 1, 1) = x;
        myhdr(i + 1, j + 1, 2) = y;
        myhdr(i + 1, j + 1, 3) = z;
    end
end

rgb = tonemap(myhdr);
figure
imshow(rgb);