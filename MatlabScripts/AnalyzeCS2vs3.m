clc ;
XDecon = [] ;
XExtract = [] ;
Xscore = [] ;

read = 0 ;
analysis1 = 0;
analysis2 = 1;


if read == 1
    fid_fout = fopen('C:\data\Dta\ExcelDocs\ScoreAnalysis.txt', 'wt') ;
    fid_extract  = fopen('C:\data\Dta\ExcelDocs\Trial.txt') ;
    fid_scores = fopen('C:\data\Dta\ExcelDocs\ScoreDistribution.txt' ) ;

    % Read in DeconMsn
    line = fgets(fid_scores) ;
    while line ~= -1
        value = str2num(line) ;
        XDecon = [XDecon ; value] ;
        line = fgets(fid_scores) ;
    end
    fclose(fid_scores) ;

    %Read in extractMSn
    line = fgets(fid_extract) ;
    while line ~= -1
        value = str2num(line) ;
        charge = value(2) ;
        if (charge == 2 || charge == 3)
            XExtract = [XExtract ; value] ;
        end
        line = fgets(fid_extract) ;
    end
    fclose(fid_extract) ;

    for i = 1 : size(XExtract,1)
        scan = XExtract(i, 1) ;
        I = find(XDecon(:,1) == scan) ;
        if (size(I) > 0)
            cs_extract = XExtract(i,2) ;
            cs_decon = XDecon(I, 2) ;
            if (cs_decon == cs_extract)
                Xscore =[Xscore ; scan cs_extract cs_decon XDecon(I,3) 1] ;
                fprintf(fid_fout, '%d\t', scan) ;
                fprintf(fid_fout, '%d\t', cs_extract) ;
                fprintf(fid_fout, '%d\t', cs_decon) ;
                fprintf(fid_fout, '%f\t', XDecon(I, 3)) ;
                fprintf(fid_fout, '%d\n', 1) ;
            else
                Xscore =[Xscore ; scan cs_extract cs_decon XDecon(I,3) 0] ;
                fprintf(fid_fout, '%d\t', scan) ;
                fprintf(fid_fout, '%d\t', cs_extract) ;
                fprintf(fid_fout, '%d\t', cs_decon) ;
                fprintf(fid_fout, '%f\t', XDecon(I, 3)) ;
                fprintf(fid_fout, '%d\n', 0) ;
            end
        end
    end

    fclose(fid_fout) ;
    save Xscore Xscore
end


if (analysis1 == 1)

    clear all
    load Xscore ;

    % the threshold way
    % we look at the score distribution for
    % distribution for all cs2 that DeconMsn found
    Ics2 = find(Xscore(:, 2) == 2) ;
    Ics3 = find(Xscore(:, 2) == 3) ;
    Xcs2 = Xscore(Ics2, :) ;
    Xcs3 = Xscore(Ics3, :) ;
    Ics2correct = find(Xcs2(:,5) == 1) ; %DeconMSn got same as ExtractMsn
    Ics3wrong = find(Xcs3(:, 5) == 0 ) ; % DeconMsn got different from ExtractMsn
    Xcs2_correct = Xcs2(Ics2correct, :) ;
    Xcs3_wrong = Xcs3(Ics3wrong, :) ;
    Xcs2_correct_scores = Xcs2(Ics2correct, 4) ;
    Xcs3_wrong_scores = Xcs3(Ics3wrong, 4) ;
    max_score = max(Xcs2_correct_scores) ;
    min_score = min(Xcs2_correct_scores) ;
    if (max(Xcs3_wrong_scores) > max_score)
        max_score = max(Xcs3_wrong_scores) ;
    end
    if (min(Xcs3_wrong_scores) < min_score)
        min_score = min(Xcs3_wrong_scores) ;
    end
    resolution = (max_score - min_score)/100;
    bins = [min_score:resolution:max_score] ;
    hist_xcs2_correct_scores = hist(Xcs2_correct_scores, bins) ;
    figure ;
    plot(bins, hist_xcs2_correct_scores,'r') ;
    hold on ;
    hist_xcs3_wrong_scores = hist(Xcs3_wrong_scores, bins) ;
    plot(bins, hist_xcs3_wrong_scores, 'b') ;
    title('SVM score distribution for +2 found by DeconMSn') ;
    legend('extract\_msn gives +2', 'extract\_msn gives +3') ;
    xlabel('Scores') ;
    ylabel('Count') ;

    %distribution on charge state 3  that DeconMSn found
    Ics3correct = find(Xcs3(:,5) == 1) ;
    Ics2wrong = find(Xcs2(:, 5) == 0 ) ;
    Xcs3_correct = Xcs3(Ics3correct, :) ;
    Xcs2_wrong = Xcs2(Ics2wrong, :) ;
    Xcs3_correct_scores = Xcs3(Ics3correct, 4) ;
    Xcs2_wrong_scores = Xcs2(Ics2wrong, 4) ;
    max_score = max(Xcs3_correct_scores) ;
    min_score = min(Xcs3_correct_scores) ;
    if (max(Xcs2_wrong_scores) > max_score)
        max_score = max(Xcs2_wrong_scores) ;
    end
    if (min(Xcs2_wrong_scores) < min_score)
        min_score = min(Xcs2_wrong_scores) ;
    end
    resolution = (max_score - min_score)/100;
    bins = [min_score:resolution:max_score] ;
    hist_xcs3_correct_scores = hist(Xcs3_correct_scores, bins) ;
    figure ;
    plot(bins, hist_xcs3_correct_scores) ;
    hold on ;
    hist_xcs2_wrong_scores = hist(Xcs2_wrong_scores, bins) ;
    plot(bins, hist_xcs2_wrong_scores, 'r') ;
    title('SVM score distribution for +3 found by DeconMSn') ;
    legend('extract\_msn gives +3', 'extract\_msn gives +2') ;
    xlabel('Scores') ;
    ylabel('Count') ;

end

if (analysis2 == 1)
    clear all
    load Xscore ;

    % the probability way
    % distribution for all cs2 and cs3 that extract_msn found (true data)
    Ics2 = find(Xscore(:, 2) == 2) ;
    Ics3 = find(Xscore(:, 2) == 3) ;
    Xcs2 = Xscore(Ics2, 4) ;
    Xcs3 = Xscore(Ics3, 4) ;
    max_score = max(Xcs2) ;
    min_score = min(Xcs2) ;
    resolution = (max_score - min_score)/100;
    bins = [min_score:resolution:max_score] ;
    hist_xcs2 = hist(Xcs2, bins) ;

    figure ;
    plot(bins, hist_xcs2/max(hist_xcs2),'r') ;
    hold on ;

    max_score = max(Xcs3) ;
    min_score = min(Xcs3) ;
    resolution = (max_score - min_score)/100;
    bins = [min_score:resolution:max_score] ;
    hist_xcs3 = hist(Xcs3, bins) ;
    plot(bins, hist_xcs3/max(hist_xcs3), 'b') ;
%     title('SVM score distribution for all +2/+3 peptides found by extract\_msn') ;
%     legend('+2 found', '+3 found') ;
%     xlabel('Scores') ;
%     ylabel('Count') ;

    %Analyzing fpr vs tpr 
    fpr_array = [] ;
    tpr_array = [] ;
    th_array = [] ; 
    step = 0.5 ; 

    %for cs 2    

    for threshold = -5:step:3
        th_array = [th_array threshold] ; 
        %false positive
        I = find(bins > threshold) ;
        fp = sum(hist_xcs3(I)) ;

        %true positive
        I = find(bins > threshold) ;
        tp = sum(hist_xcs2(I)) ;

        fpr = fp/(fp+tp) ;
        tpr = tp/(fp+tp) ;
        
        fpr_array = [fpr_array fpr] ; 
        tpr_array = [tpr_array tpr] ; 
    end
    figure ;
    tparr =  sort(tpr_array) ; 
    fparr =  sort(fpr_array) ; 
    plot(tparr, fparr) ; 
    xlabel('True Positive Rate') ; 
    ylabel('False Positive Rate') ; 
   % hold on ;     
    
    figure ; 
    plot(th_array, tpr_array) ; 
    hold on ; 
    plot(th_array, fpr_array, 'r') ; 
     
%     %for cs 3
%     step = -0.5 ; 
%     for threshold = 0:step:-5
%         %false positive
%         I = find(bins<threshold) ; 
%         fp = sum(hist_xcs2(I)) ; 
%         
%         %true positive
%         tp = sum(hist_xcs3(I)) ; 
%         
%         fpr = fp/(fp+tp) ;
%         tpr = tp/(fp+tp) ; 
%         
%         fpr_array = [fpr_array fpr] ; 
%         tpr_array = [tpr_array tpr] ; 
%     end   
%     tprarr = sort(tpr_array) ; 
%     fprarr = sort(fpr_array) ; 
%     plot(tprarr, fprarr, 'r') ; 
%     legend('cs +2', 'cs +3') ;       
%      
     
end


