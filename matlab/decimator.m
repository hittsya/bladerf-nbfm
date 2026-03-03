% Generate one pseudo FM signal at 0 offset from baseband
% and other one in the offset of 300 kHz
function x = gen_fm_signal(Fs, t)
    fm1a = 2e3;
    fm1b = 7e3;
    m1 = 0.7*sin(2*pi*fm1a*t) + 0.5*sin(2*pi*fm1b*t);

    f_dev1 = 75e3;
    phase1 = 2*pi*f_dev1 * cumsum(m1)/Fs;
    s1 = exp(1j*phase1);

    fm2a = 3e3;
    fm2b = 9e3;
    m2 = 0.6*sin(2*pi*fm2a*t) + 0.4*sin(2*pi*fm2b*t);

    f_dev2 = 75e3;
    phase2 = 2*pi*f_dev2 * cumsum(m2)/Fs;
    s2 = exp(1j*phase2);

    f_shift = 300e3;
    s2 = s2 .* exp(1j*2*pi*f_shift*t);

    x = s1 + s2;
    x = x + 0.02*(randn(size(x)) + 1j*randn(size(x)));
end

% Generate reference signal, which was used to compare
% sample-by-sample with the c++ implementation
function x = gen_fm_signal_test(Fs, t)
    f1 = 1e3;
    f2 = 60e3;
    f3 = 110e3;
    f4 = 180e3;
    f5 = 320e3;

    x = 0.8*exp(1j*2*pi*f1*t) + ...
        0.6*exp(1j*2*pi*f2*t) + ...
        0.6*exp(1j*2*pi*f3*t) + ...
        0.5*exp(1j*2*pi*f4*t) + ...
        0.5*exp(1j*2*pi*f5*t);
end

function [x, Fs_new] = decimate_sig(Fs, M, sig)
    x = sig(1:M:end);
    Fs_new = Fs/M;
end

function b = fir_lowpass(Fc, Fs, order)
    Fc = (Fc / (Fs/2)) / 2;
    n = 0:order;
    alpha = order/2;
    h = zeros(size(n));

    for k = 1:length(n)
        if n(k) == alpha
            h(k) = 2*Fc;
        else
            h(k) = sin(2*pi*Fc*(n(k)-alpha)) / (pi*(n(k)-alpha));
        end
    end

    wnd = 0.54 - 0.46*cos(2*pi*n/order);
    b = h .* wnd;
    b = b / sum(b);

    disp([real(b(1:50)).', imag(b(1:50)).']);
end

function main
    clc; clear;

    Fs = 1e6;
    N = 2^14;
    t = (0:N-1)/Fs;

    x = gen_fm_signal(Fs, t);
    %x = gen_fm_signal_test(Fs, t);

    fig = uifigure('Name','Decimator','Position',[100 100 1000 860]);

    ax1 = uiaxes(fig,'Position',[0 600 1000 250]);
    title(ax1,'Original Spectrum')

    ax2 = uiaxes(fig,'Position',[0 320 1000 250]);
    title(ax2,'After Decimation')

    ax3 = uiaxes(fig,'Position',[0 40  1000 250]);
    title(ax3,'After FIR')

    updatePlot();

    function updatePlot()
        % orig
        X = fftshift(abs(fft(x)));
        f = linspace(-Fs/2, Fs/2, length(X));
        plot(ax1, f/1e3, 20*log10(X/max(X)));
        xlabel(ax1,'Frequency');
        ylabel(ax1,'Magnitude');
        grid(ax1,'on');

        % decimated
        [decimated, Fs_new] = decimate_sig(Fs, 6, x);

        Y = fftshift(abs(fft(decimated)));
        f2 = linspace(-Fs_new/2, Fs_new/2, length(Y));

        plot(ax2, f2/1e3, 20*log10(Y/max(Y)));
        xlabel(ax2,'Frequency');
        ylabel(ax2,'Magnitude');
        grid(ax2,'on');

        % filtered
        Fc = 100 * 1e3;
        %b = fir1(128, Fc/(Fs/2));
        b = fir_lowpass(Fc, Fs, 128);
        xf = filter(b,1,x);
        [y, Fs_new] = decimate_sig(Fs, 6, xf);

        %disp([real(y(1:50)).', imag(y(1:50)).']);

        Y = fftshift(abs(fft(y)));
        f2 = linspace(-Fs_new/2, Fs_new/2 + 100*1e3, length(Y));
        plot(ax3, f2/1e3, 20*log10(Y/max(Y)));
        xlabel(ax3,'Frequency');
        ylabel(ax3,'Magnitude');
        grid(ax3,'on');
    end
end

main()
