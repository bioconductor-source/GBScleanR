// [[depends(RcppParallel)]]
// [[Rcpp::plugins("cpp11")]]
#include <RcppParallel.h>
#include <Rcpp.h>
#include <random>
#include <vector>
#include <algorithm>
using namespace Rcpp;
using namespace RcppParallel;
using std::vector;

// Calculate the log10 value
void log10_safe(double & d){
  if(d == 0){
    d = -pow(10, 100);
  } else {
    d = log10(d);
  }
}

double logsum(std::vector<double> & v){
  if(v.size() == 1){
    return v[0];
  }
  double log_sum;
  std::vector<double>::size_type count;
  double neg_inf = -std::numeric_limits<double>::infinity();
  double v_max = *std::max_element(v.begin(), v.end());
  if(std::isinf(v_max)){
    return neg_inf;
  };
  
  for(std::vector<double>::size_type i=0; i<v.size(); ++i){
    if(!std::isinf(v[i])){
      count = i + 1;
      log_sum = v[i];
      break;
    }
  }
  
  for(std::vector<double>::size_type i=count; i<v.size(); ++i){
    if(log_sum > v[i]){
      log_sum = log_sum + log10(1 + pow(10, v[i] - log_sum));
    } else {
      log_sum = v[i] + log10(1 + pow(10, log_sum - v[i]));
    }
  }
  return log_sum;
}

// Normalize log probabilities
NumericVector lognorm(NumericVector v){
  double log_sum;
  std::vector<double>::size_type count;
  double v_max = *std::max_element(v.begin(), v.end());
  if(std::isinf(v_max)){
    double even = 1 / (double)v.size();
    log10_safe(even);
    v.fill(even);
    return v;
  };
  for(R_xlen_t i=0; i<v.size(); ++i){
    if(!std::isinf(v[i])){
      count = i + 1;
      log_sum = v[i];
      break;
    }
  }
  
  for(R_xlen_t i=count; i<v.size(); ++i){
    if(log_sum > v[i]){
      log_sum = log_sum + log10(1 + pow(10, v[i] - log_sum));
    } else {
      log_sum = v[i] + log10(1 + pow(10, log_sum - v[i]));
    }
  }
  
  v = v - log_sum;
  return v;
}

void lognorm_vec(vector<double> & v){
  double log_sum;
  std::vector<double>::size_type count;
  double v_max = *std::max_element(v.begin(), v.end());
  
  if(std::isinf(v_max)){
    double even = 1 / (double)v.size();
    log10_safe(even);
    for(std::vector<double>::size_type i=0; i<v.size(); ++i){
      v[i] = even;
    }
  } else {
    for(std::vector<double>::size_type i=0; i<v.size(); ++i){
      if(!std::isinf(v[i])){
        count = i + 1;
        log_sum = v[i];
        break;
      }
    }
    
    for(std::vector<double>::size_type i=count; i<v.size(); ++i){
      if(log_sum > v[i]){
        log_sum = log_sum + log10(1 + pow(10, v[i] - log_sum));
      } else {
        log_sum = v[i] + log10(1 + pow(10, log_sum - v[i]));
      }
    }
    
    for(std::vector<double>::size_type i=0; i<v.size(); ++i){
      v[i] = v[i] - log_sum;
    }
  }
}

// Power to ten
double pow10(double & d){
  return pow(10, d);
}

std::size_t get_max_int(std::vector<double> & v){
  std::size_t out_index;
  std::vector<std::size_t> max_indices;
  double v_max;
  bool check;
  v_max = *std::max_element(v.begin(), v.end());
  
  for(std::vector<double>::size_type l=0;l<v.size(); ++l){
    check = fabs(v.at(l) - v_max) < 0.0000000001;
    if(check){
      max_indices.push_back(l);
    }
  }
  
  if(max_indices.size() == 0){
    std::random_device rnd;
    std::mt19937 mt(rnd());
    int tmp_len = v.size();
    std::uniform_int_distribution<> rand1(0, tmp_len - 1);
    out_index = rand1(mt);
    return out_index;
  }
  
  if(max_indices.size() == 1){
    return max_indices[0];
  }
  
  std::random_device rnd;
  std::mt19937 mt(rnd());
  int tmp_len = max_indices.size();
  std::uniform_int_distribution<> rand1(0, tmp_len - 1);
  int tmp = rand1(mt);
  out_index = max_indices[tmp];
  return out_index;
}


NumericVector calcPemit(NumericMatrix p_ref,
                        NumericMatrix p_alt,
                        NumericVector eseq,
                        NumericVector w1,
                        NumericVector w2,
                        NumericVector mismap1,
                        NumericVector mismap2,
                        IntegerVector possiblegeno,
                        int m,
                        int n_f,
                        int n_p
){
  double neg_inf = -std::numeric_limits<double>::infinity();
  double p_prob;
  int col_i;
  std::vector<double> ref_multiplier = {eseq[0], w1[m], eseq[1]};
  std::vector<double> alt_multiplier = {eseq[1], w2[m], eseq[0]};
  NumericVector p_emit(n_p, 1.0);
  
  for(int i=0; i<n_f; ++i){
    // Calculate genotype probabilies
    std::vector<double> prob_i(3);
    NumericMatrix::Row ref_i = p_ref.row(i);
    NumericMatrix::Row alt_i = p_alt.row(i);
    for(int g=0; g<3;++g){
      prob_i[g] = ref_i[m]* ref_multiplier[g] + alt_i[m] * alt_multiplier[g];
    }
    lognorm_vec(prob_i);
    
    for(int g=0; g<3;++g){
      prob_i[g] = pow10(prob_i[g]);
    }
    
    // Calculate mismap accounted genotype probabilities
    std::vector<double> v1 = {1 - mismap1[m], mismap1[m], 0};
    std::vector<double> v2 = {0, 1, 0};
    std::vector<double> v3 = {0, mismap2[m], 1 - mismap2[m]};
    double sum_v1 = 0.0;
    double sum_v2 = 0.0;
    double sum_v3 = 0.0;
    double sum_v;
    for(int g=0; g<3;++g){
      sum_v1 += v1[g] * prob_i[g];
      sum_v2 += v2[g] * prob_i[g];
      sum_v3 += v3[g] * prob_i[g];
    }
    sum_v = sum_v1 + sum_v2 + sum_v3;
    prob_i[0] = sum_v1 / sum_v;
    prob_i[1] = sum_v2 / sum_v;
    prob_i[2] = sum_v3 / sum_v;
    
    for(int j=0; j<n_p; ++j){
      col_i = j * n_f + i;
      p_prob = prob_i[possiblegeno[col_i]];
      if(p_prob < 0.01){
        p_prob = 0;
      }
      p_emit[j] = p_emit[j] * p_prob;
    }
  }
  
  for(int j=0; j<n_p; ++j){
    if(p_emit[j] == 0){
      p_emit[j] = neg_inf;
    } else {
      log10_safe(p_emit[j]);
    }
  }
  p_emit = lognorm(p_emit);
  return p_emit;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions for the Viterbi algorithm

// Initialize Viterbi scores
struct ParInitVit : public Worker {
  
  RMatrix<double> vit_score;
  const RVector<int> iter_sample;
  const RMatrix<double> ref;
  const RMatrix<double> alt;
  const RVector<double> eseq;
  const RVector<double> w1;
  const RVector<double> w2;
  const RVector<double> mismap1;
  const RVector<double> mismap2;
  const RVector<int> possiblehap;
  const RVector<double> init_prob;
  const RVector<int> dim;
  const RVector<int> valid_p_indices;
  
  
  ParInitVit(NumericMatrix vit_score,
             const LogicalVector iter_sample,
             const NumericMatrix ref,
             const NumericMatrix alt,
             const NumericVector eseq,
             const NumericVector w1,
             const NumericVector w2,
             const NumericVector mismap1,
             const NumericVector mismap2,
             const IntegerVector possiblehap,
             const NumericVector init_prob,
             const IntegerVector dim,
             const IntegerVector valid_p_indices)
    : vit_score(vit_score),
      iter_sample(iter_sample),
      ref(ref),
      alt(alt),
      eseq(eseq),
      w1(w1),
      w2(w2),
      mismap1(mismap1),
      mismap2(mismap2),
      possiblehap(possiblehap),
      init_prob(init_prob),
      dim(dim),
      valid_p_indices(valid_p_indices) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_sample.begin() + begin; i<iter_sample.begin() + end; ++i){
      std::size_t sample_i = std::distance(iter_sample.begin(), i);
      RMatrix<double>::Row vit_i = vit_score.row(sample_i);
      
      // Calculate genotype probabilies
      std::vector<double> prob_i(3);
      std::vector<double> ref_multiplier = {eseq[0], w1[0], eseq[1]};
      std::vector<double> alt_multiplier = {eseq[1], w2[0], eseq[0]};
      RMatrix<double>::Row ref_i = ref.row(sample_i);
      RMatrix<double>::Row alt_i = alt.row(sample_i);
      for(int g=0; g<3;++g){
        prob_i[g] = ref_i[0]* ref_multiplier[g] + alt_i[0] * alt_multiplier[g];
      }
      lognorm_vec(prob_i);
      for(int g=0; g<3;++g){
        prob_i[g] = pow10(prob_i[g]);
      }
      
      // Calculate mismap accounted genotype probabilities
      std::vector<double> v1 = {1 - mismap1[0], mismap1[0], 0};
      std::vector<double> v2 = {0, 1, 0};
      std::vector<double> v3 = {0, mismap2[0], 1 - mismap2[0]};
      double sum_v1 = 0.0;
      double sum_v2 = 0.0;
      double sum_v3 = 0.0;
      double sum_v = 0.0;
      
      for(std::size_t g=0; g<3;++g){
        sum_v1 += v1[g] * prob_i[g];
        sum_v2 += v2[g] * prob_i[g];
        sum_v3 += v3[g] * prob_i[g];
      }
      sum_v = sum_v1 + sum_v2 + sum_v3;
      prob_i[0] = sum_v1 / sum_v;
      prob_i[1] = sum_v2 / sum_v;
      prob_i[2] = sum_v3 / sum_v;
      double hap_prob = 0.0;
      std::size_t col_i;
      std::size_t n_h = dim[4];
      std::size_t valid_j;
      
      for(std::size_t j=0; j<valid_p_indices.size(); ++j){
        for(std::size_t k=0; k<n_h; ++k){
          valid_j = valid_p_indices[j];
          col_i = valid_j * n_h + k;
          hap_prob = prob_i[possiblehap[col_i]];
          log10_safe(hap_prob);
          col_i = j * n_h + k;
          vit_i[col_i] = hap_prob + init_prob[k];
        }
      }
    }
  }
};

// Calculate Viterbi scores
struct ParCalcVit : public Worker {
  
  RMatrix<double> in_score;
  RMatrix<int> tmp_path;
  const RVector<int> iter_sample;
  const RMatrix<double> trans_prob_m;
  const RVector<int> dim;
  const RVector<int> valid_p_indices;
  
  ParCalcVit(NumericMatrix in_score,
             IntegerMatrix tmp_path,
             const LogicalVector iter_sample,
             const NumericMatrix trans_prob_m,
             const IntegerVector dim,
             const IntegerVector valid_p_indices)
    : in_score(in_score),
      tmp_path(tmp_path),
      iter_sample(iter_sample),
      trans_prob_m(trans_prob_m),
      dim(dim),
      valid_p_indices(valid_p_indices) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_sample.begin() + begin; i<iter_sample.begin() + end; ++i){
      std::size_t sample_i = std::distance(iter_sample.begin(), i);
      RMatrix<double>::Row in_i = in_score.row(sample_i);
      RMatrix<int>::Row tmp_i = tmp_path.row(sample_i);
      
      std::size_t col_i;
      std::size_t n_p = dim[3];
      std::size_t n_h = dim[4];
      double trans_kk;
      std::size_t max_i;
      std::vector<double> score_jkk(n_h);
      std::vector<double> max_scores_jk(n_h);
      
      for(std::size_t j=0; j<n_p; ++j){
        
        std::size_t valid_j1;
        for(std::size_t l=0;l<valid_p_indices.size();++l){
          valid_j1 = valid_p_indices[l];
          if(j == valid_j1){
            valid_j1 = l;
            for(std::size_t k2=0; k2<n_h; ++k2){
              RMatrix<double>::Column trans_prob_k = trans_prob_m.column(k2);
              for(std::size_t k1=0; k1<n_h; ++k1){
                col_i = valid_j1 * n_h + k1;
                trans_kk = trans_prob_k[k1];
                score_jkk[k1] = in_i[col_i] + trans_kk;
              }
              max_i = get_max_int(score_jkk);
              tmp_i[j * n_h + k2] = max_i;
              max_scores_jk[k2] = score_jkk[max_i];
            }
            for(std::size_t k2=0; k2<n_h; ++k2){
              in_i[valid_j1 * n_h + k2] = max_scores_jk[k2];
            }
          }
        }
      }
    }
  }
};

// Calculate Founder Viterbi scores
struct ParCalcPath : public Worker {
  
  RMatrix<int> f_path;
  RMatrix<int> o_path;
  RMatrix<double> vit_score;
  const RMatrix<double> in_score;
  const RMatrix<int> tmp_path;
  const RVector<int> iter_p_pat;
  const RMatrix<double> ref;
  const RMatrix<double> alt;
  const RVector<double> eseq;
  const RVector<double> w1;
  const RVector<double> w2;
  const RVector<double> mismap1;
  const RVector<double> mismap2;
  const RVector<int> possiblehap;
  const RVector<int> dim;
  const RVector<double> p_emit1;
  const RVector<double> p_emit2;
  const RVector<int> valid_p_indices1;
  const RVector<int> valid_p_indices2;
  const RVector<int> m;
  
  ParCalcPath(IntegerMatrix f_path,
              IntegerMatrix o_path,
              NumericMatrix vit_score,
              const NumericMatrix in_score,
              const IntegerMatrix tmp_path,
              const LogicalVector iter_p_pat,
              const NumericMatrix ref,
              const NumericMatrix alt,
              const NumericVector eseq,
              const NumericVector w1,
              const NumericVector w2,
              const NumericVector mismap1,
              const NumericVector mismap2,
              const IntegerVector possiblehap,
              const IntegerVector dim,
              const NumericVector p_emit1,
              const NumericVector p_emit2,
              const IntegerVector valid_p_indices1,
              const IntegerVector valid_p_indices2,
              const IntegerVector m)
    : f_path(f_path),
      o_path(o_path),
      vit_score(vit_score),
      in_score(in_score),
      tmp_path(tmp_path),
      iter_p_pat(iter_p_pat),
      ref(ref),
      alt(alt),
      eseq(eseq),
      w1(w1),
      w2(w2),
      mismap1(mismap1),
      mismap2(mismap2),
      possiblehap(possiblehap),
      dim(dim),
      p_emit1(p_emit1),
      p_emit2(p_emit2),
      valid_p_indices1(valid_p_indices1),
      valid_p_indices2(valid_p_indices2),
      m(m) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_p_pat.begin() + begin; i<iter_p_pat.begin() + end; ++i){
      RMatrix<int>::Row f_path_m = f_path.row(m[0]);
      RMatrix<int>::Row o_path_m = o_path.row(m[0]);
      
      std::size_t j2 = std::distance(iter_p_pat.begin(), i);
      double neg_inf = -std::numeric_limits<double>::infinity();
      std::vector<double> prob_i(3);
      double hap_prob;
      std::size_t n_o = dim[2];
      std::size_t n_p = dim[3];
      std::size_t n_h = dim[4];
      std::size_t per_sample_size = n_p*n_h;
      std::size_t col_i;
      std::vector<double> ref_multiplier;
      std::vector<double> alt_multiplier;
      std::vector<double> v1;
      std::vector<double> v2;
      std::vector<double> v3;
      std::vector<double> score_j(n_p);
      std::vector<double> score_ijk(n_h);
      double sum_v;
      std::size_t max_j;
      std::size_t max_valid_index;
      
      
      if(std::isinf(p_emit2[j2])){
        f_path_m[j2] = -1;
        
      } else {
        for(std::size_t sample_i=0; sample_i<n_o; ++sample_i){
          RMatrix<double>::Row in_i = in_score.row(sample_i);
          
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[m[0]], eseq[1]};
          alt_multiplier = {eseq[1], w2[m[0]], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[m[0]] * ref_multiplier[g] + alt_i[m[0]] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[m[0]], mismap1[m[0]], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[m[0]], 1 - mismap2[m[0]]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          for(std::size_t j1=0; j1<n_p; ++j1){
            
            if(std::isinf(p_emit1[j1])){
              score_j[j1] = neg_inf;
            } else {
              
              std::size_t valid_j1;
              for(std::size_t j=0;j<valid_p_indices1.size();++j){
                valid_j1 = valid_p_indices1[j];
                if(j1 == valid_j1){
                  valid_j1 = j;
                  break;
                }
              }
              
              for(std::size_t k=0; k<n_h; ++k){
                hap_prob = prob_i[possiblehap[j2 * n_h + k]];
                log10_safe(hap_prob);
                score_ijk[k] = in_i[valid_j1 * n_h + k] + hap_prob;
              }
              score_j[j1] += logsum(score_ijk);
            }
          }
        }
        for(std::size_t j1=0; j1<n_p; ++j1){
          score_j[j1] = p_emit1[j1] + score_j[j1];
        }
        max_j = get_max_int(score_j);
        f_path_m[j2] = max_j;
        std::size_t valid_j1;
        for(std::size_t j=0;j<valid_p_indices1.size();++j){
          valid_j1 = valid_p_indices1[j];
          if(max_j == valid_j1){
            max_valid_index = j;
          }
        }
        
        std::size_t j2_valid_index;
        std::size_t valid_j2;
        for(std::size_t j=0;j<valid_p_indices2.size();++j){
          valid_j2 = valid_p_indices2[j];
          if(j2 == valid_j2){
            j2_valid_index = j;
          }
        }
        
        for(std::size_t sample_i=0; sample_i<n_o; ++sample_i){
          RMatrix<double>::Row vit_i = vit_score.row(sample_i);
          RMatrix<double>::Row in_i = in_score.row(sample_i);
          RMatrix<int>::Row tmp_i = tmp_path.row(sample_i);
          
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[m[0]], eseq[1]};
          alt_multiplier = {eseq[1], w2[m[0]], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[m[0]] * ref_multiplier[g] + alt_i[m[0]] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[m[0]], mismap1[m[0]], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[m[0]], 1 - mismap2[m[0]]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          for(std::size_t k=0; k<n_h; ++k){
            col_i = j2 * n_h + k;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            o_path_m[sample_i * per_sample_size + col_i] = tmp_i[max_j * n_h + k];
            vit_i[j2_valid_index * n_h + k] = in_i[max_valid_index * n_h + k] + hap_prob;
          }
        }
      }
    }
  }
};

// Calculate alpha values in the forward algorithm
void last_vit(IntegerVector f_seq,
              IntegerMatrix o_seq,
              NumericMatrix vit_score,
              NumericVector p_emit,
              IntegerVector dim,
              IntegerVector valid_p_indices){
  std::size_t n_o = dim[2];
  std::size_t n_p = dim[3];
  std::size_t n_h = dim[4];
  std::size_t m = dim[0]-1;
  std::size_t vit_i;
  double neg_inf = -std::numeric_limits<double>::infinity();
  
  // Calculate emit, alpha, and gamma
  IntegerMatrix tmp_argmax(n_o, valid_p_indices.size());
  std::vector<double> score_ijk(n_h);
  std::vector<double> score_j(n_p);
  
  for(std::size_t i=0; i<n_o; ++i){
    for(std::size_t j1=0; j1<n_p; ++j1){
      if(std::isinf(p_emit[j1])){
        score_j[j1] = neg_inf;
      } else {
        
        std::size_t valid_j1;
        for(R_xlen_t j=0;j<valid_p_indices.size();++j){
          valid_j1 = valid_p_indices[j];
          if(j1 == valid_j1){
            valid_j1 = j;
            break;
          }
        }
        for(std::size_t k=0; k<n_h; ++k){
          vit_i = valid_j1 * n_h + k;
          score_ijk[k] = vit_score(i, vit_i);
        }
        score_j[j1] += logsum(score_ijk);
        tmp_argmax( i , valid_j1 ) = get_max_int(score_ijk);
      }
    }
  }
  for(std::size_t j=0; j<n_p; ++j){
    score_j[j] = p_emit[j] + score_j[j];
  }
  std::size_t max_j = get_max_int(score_j);
  f_seq[m] = max_j;
  
  std::size_t tmp_j;
  std::size_t max_valid_index;
  for(R_xlen_t j=0;j<valid_p_indices.size();++j){
    tmp_j = valid_p_indices[j];
    if(max_j == tmp_j){
      max_valid_index = j;
    }
  }
  
  for(std::size_t i=0; i<n_o; ++i){
    o_seq( m , i ) = tmp_argmax( i , max_valid_index );
  }
}

// Find the best state sequences backwardly
void backtrack(IntegerMatrix f_path,
               IntegerMatrix o_path,
               IntegerVector f_seq,
               IntegerMatrix o_seq,
               IntegerVector dim){
  std::size_t f_prev;
  std::size_t o_prev;
  std::size_t n_m = dim[0];
  std::size_t n_o = dim[2];
  std::size_t n_p = dim[3];
  std::size_t n_h = dim[4];
  std::size_t per_sample_size = n_p * n_h;
  
  for(std::size_t m=n_m-1; m>0; --m){
    if(m % 10 == 9){
      Rcpp::Rcout << "\r" << "Backtracking best genotype sequences at marker#: " << m+1 << std::string(70, ' ');
    }
    f_prev = f_seq[m];
    f_seq[m-1] = f_path(m , f_prev);
    for(std::size_t i=0; i<n_o; ++i){
      o_prev = o_seq( m , i );
      o_seq( m-1 , i ) = o_path(m, i * per_sample_size + f_prev * n_h + o_prev);
    }
  }
  Rcpp::Rcout << "\r" << "Backtracking best genotype sequences: Dene!" << std::string(70, ' ');
}


// Solve the HMM
// [[Rcpp::export]]
List run_viterbi(NumericMatrix p_ref,
                 NumericMatrix p_alt,
                 NumericMatrix ref,
                 NumericMatrix alt,
                 NumericVector eseq_in,
                 NumericVector bias,
                 NumericMatrix mismap,
                 NumericMatrix trans_prob,
                 NumericVector init_prob,
                 int & n_p,
                 int & n_h,
                 int & n_o,
                 int & n_f,
                 int & n_m,
                 IntegerVector possiblehap,
                 IntegerVector possiblegeno,
                 IntegerVector p_geno_fix
){
  // Initialize arrays to store output, alpha values, emittion probs, and beta values.
  double neg_inf = -std::numeric_limits<double>::infinity();
  IntegerMatrix o_seq(n_m, n_o);
  IntegerVector f_seq(n_m);
  
  IntegerMatrix f_path(n_m, n_p);
  IntegerMatrix o_path(n_m, n_o * n_p * n_h);
  IntegerMatrix tmp_path(n_o, n_p * n_h);
  
  // Convert values to ones used here.
  IntegerVector dim = {n_m, n_f, n_o, n_p, n_h};
  NumericVector w1(n_m);
  NumericVector w2(n_m);
  NumericVector eseq(2);
  NumericVector mismap1 = mismap( _ , 0 );
  NumericVector mismap2 = mismap( _ , 1 );
  eseq = clone(eseq_in);
  
  for(R_xlen_t i=0; i<eseq.size(); ++i){
    log10_safe(eseq[i]);
  }
  w1 = clone(bias);
  w2 = clone(bias);
  w2 = 1 - w2;
  for(R_xlen_t i=0; i<w1.size(); ++i){
    log10_safe(w1[i]);
    log10_safe(w2[i]);
  }
  
  LogicalVector iter_sample(n_o);
  LogicalVector iter_p_pat(n_p);
  NumericVector p_emit1;
  NumericVector p_emit2;
  
  // Initialize Viterbi trellis.
  int m = 0;
  p_emit1 = calcPemit(p_ref,
                      p_alt,
                      eseq,
                      w1,
                      w2,
                      mismap1,
                      mismap2,
                      possiblegeno,
                      m,
                      n_f,
                      n_p);
  
  int int_fix_p = p_geno_fix[0];
  if(int_fix_p >= 0){
    R_xlen_t fix_p = p_geno_fix[0];
    for(R_xlen_t p=0; p<p_emit1.size(); ++p){
      if(p == fix_p){
        p_emit1[p] = 0;
      } else {
        p_emit1[p] = neg_inf;
      }
    }
  }
  
  IntegerVector valid_p_indices1;
  for(R_xlen_t j=0; j<p_emit1.size(); ++j){
    if(!std::isinf(p_emit1[j])){
      valid_p_indices1.push_back(j);
    }
  }
  std::size_t valid_size;
  valid_size = valid_p_indices1.size();
  NumericMatrix vit_score(n_o, valid_size * n_h);
  ParInitVit init_vit(vit_score,
                      iter_sample,
                      ref,
                      alt,
                      eseq,
                      w1,
                      w2,
                      mismap1,
                      mismap2,
                      possiblehap,
                      init_prob,
                      dim,
                      valid_p_indices1);
  parallelFor(0, iter_sample.length(), init_vit);
  NumericMatrix in_score;
  in_score = clone(vit_score);
  
  for(int m=1; m<n_m; ++m){
    
    if(m % 10 == 9){
      Rcpp::Rcout << "\r" << "Forward founder genotype probability calculation at marker#: " << m+1 << std::string(70, ' ');
    }
    NumericMatrix trans_prob_m = trans_prob( _ , Range( (m-1)*n_h , (m-1)*n_h + n_h - 1 ) );
    
    ParCalcVit calc_vit(in_score,
                        tmp_path,
                        iter_sample,
                        trans_prob_m,
                        dim,
                        valid_p_indices1);
    parallelFor(0, iter_sample.length(), calc_vit);
    
    p_emit2 = calcPemit(p_ref,
                        p_alt,
                        eseq,
                        w1,
                        w2,
                        mismap1,
                        mismap2,
                        possiblegeno,
                        m,
                        n_f,
                        n_p);
    
    R_xlen_t fix_p_len = p_geno_fix.size();
    if(fix_p_len > m){
      R_xlen_t fix_p = p_geno_fix[m];
      for(R_xlen_t p=0; p<p_emit2.size(); ++p){
        if(p == fix_p){
          p_emit2[p] = 0;
        } else {
          p_emit2[p] = neg_inf;
        }
      }
    };
    
    IntegerVector valid_p_indices2;
    for(R_xlen_t j=0; j<p_emit2.size(); ++j){
      if(!std::isinf(p_emit2[j])){
        valid_p_indices2.push_back(j);
      }
    }
    
    valid_size = valid_p_indices2.size();
    NumericMatrix vit_score(n_o, valid_size * n_h);
    IntegerVector in_m = {m};
    ParCalcPath calc_path(f_path,
                          o_path,
                          vit_score,
                          in_score,
                          tmp_path,
                          iter_p_pat,
                          ref,
                          alt,
                          eseq,
                          w1,
                          w2,
                          mismap1,
                          mismap2,
                          possiblehap,
                          dim,
                          p_emit1,
                          p_emit2,
                          valid_p_indices1,
                          valid_p_indices2,
                          in_m);
    
    parallelFor(0, iter_p_pat.length(), calc_path);
    p_emit1 = clone(p_emit2);
    valid_p_indices1 = clone(valid_p_indices2);
    in_score = clone(vit_score);
    
    if(m==n_m-1){
      last_vit(f_seq,
               o_seq,
               in_score,
               p_emit1,
               dim,
               valid_p_indices1);
    }
  }
  Rcpp::Rcout << "\r" << "Forward founder genotype probability calculation: Dene!" << std::string(70, ' ');
  
  backtrack(f_path,
            o_path,
            f_seq,
            o_seq,
            dim);
  
  Rcpp::Rcout << "\r" << std::string(70, ' ');
  List out_list = List::create(_["p_geno"] = f_seq, _["best_seq"] = o_seq);
  return out_list;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Run Viterbi algorithm for founder genotype and offspring genotype separately


// Calculate Viterbi scores
struct ParCalcVitFounder : public Worker {
  
  RMatrix<double> in_score;
  const RVector<int> iter_sample;
  const RMatrix<double> trans_prob_m;
  const RVector<int> dim;
  const RVector<int> valid_p_indices;
  
  ParCalcVitFounder(NumericMatrix in_score,
                    const LogicalVector iter_sample,
                    const NumericMatrix trans_prob_m,
                    const IntegerVector dim,
                    const IntegerVector valid_p_indices)
    : in_score(in_score),
      iter_sample(iter_sample),
      trans_prob_m(trans_prob_m),
      dim(dim),
      valid_p_indices(valid_p_indices) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_sample.begin() + begin; i<iter_sample.begin() + end; ++i){
      std::size_t sample_i = std::distance(iter_sample.begin(), i);
      RMatrix<double>::Row in_i = in_score.row(sample_i);
      
      std::size_t col_i;
      std::size_t n_h = dim[4];
      double trans_kk;
      std::size_t max_i;
      std::vector<double> score_jkk(n_h);
      std::vector<double> max_scores_jk(n_h);
      
      for(std::size_t j=0; j<valid_p_indices.size(); ++j){
        for(std::size_t k2=0; k2<n_h; ++k2){
          RMatrix<double>::Column trans_prob_k = trans_prob_m.column(k2);
          for(std::size_t k1=0; k1<n_h; ++k1){
            col_i = j * n_h + k1;
            trans_kk = trans_prob_k[k1];
            score_jkk[k1] = in_i[col_i] + trans_kk;
          }
          max_i = get_max_int(score_jkk);
          max_scores_jk[k2] = score_jkk[max_i];
        }
        for(std::size_t k2=0; k2<n_h; ++k2){
          col_i = j * n_h + k2;
          in_i[col_i] = max_scores_jk[k2];
        }
      }
    }
  }
};

// Calculate Founder Viterbi scores
struct ParCalcPathFounder : public Worker {
  
  RMatrix<int> f_path;
  RMatrix<double> vit_score;
  const RMatrix<double> in_score;
  const RVector<int> iter_p_pat;
  const RMatrix<double> ref;
  const RMatrix<double> alt;
  const RVector<double> eseq;
  const RVector<double> w1;
  const RVector<double> w2;
  const RVector<double> mismap1;
  const RVector<double> mismap2;
  const RVector<int> possiblehap;
  const RVector<int> dim;
  const RVector<double> p_emit1;
  const RVector<double> p_emit2;
  const RVector<int> valid_p_indices1;
  const RVector<int> valid_p_indices2;
  const RVector<int> m;
  
  ParCalcPathFounder(IntegerMatrix f_path,
                     NumericMatrix vit_score,
                     const NumericMatrix in_score,
                     const LogicalVector iter_p_pat,
                     const NumericMatrix ref,
                     const NumericMatrix alt,
                     const NumericVector eseq,
                     const NumericVector w1,
                     const NumericVector w2,
                     const NumericVector mismap1,
                     const NumericVector mismap2,
                     const IntegerVector possiblehap,
                     const IntegerVector dim,
                     const NumericVector p_emit1,
                     const NumericVector p_emit2,
                     const IntegerVector valid_p_indices1,
                     const IntegerVector valid_p_indices2,
                     const IntegerVector m)
    : f_path(f_path),
      vit_score(vit_score),
      in_score(in_score),
      iter_p_pat(iter_p_pat),
      ref(ref),
      alt(alt),
      eseq(eseq),
      w1(w1),
      w2(w2),
      mismap1(mismap1),
      mismap2(mismap2),
      possiblehap(possiblehap),
      dim(dim),
      p_emit1(p_emit1),
      p_emit2(p_emit2),
      valid_p_indices1(valid_p_indices1),
      valid_p_indices2(valid_p_indices2),
      m(m) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_p_pat.begin() + begin; i<iter_p_pat.begin() + end; ++i){
      RMatrix<int>::Row f_path_m = f_path.row(m[0]);
      
      std::size_t j2 = std::distance(iter_p_pat.begin(), i);
      double neg_inf = -std::numeric_limits<double>::infinity();
      std::vector<double> prob_i(3);
      double hap_prob;
      std::size_t n_o = dim[2];
      std::size_t n_p = dim[3];
      std::size_t n_h = dim[4];
      std::size_t col_i;
      std::vector<double> ref_multiplier;
      std::vector<double> alt_multiplier;
      std::vector<double> v1;
      std::vector<double> v2;
      std::vector<double> v3;
      std::vector<double> score_j(n_p);
      std::vector<double> score_ijk(n_h);
      double sum_v;
      std::size_t max_j;
      std::size_t col_in;
      
      
      if(std::isinf(p_emit2[j2])){
        f_path_m[j2] = -1;
        
      } else {
        for(std::size_t sample_i=0; sample_i<n_o; ++sample_i){
          RMatrix<double>::Row in_i = in_score.row(sample_i);
          
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[m[0]], eseq[1]};
          alt_multiplier = {eseq[1], w2[m[0]], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[m[0]] * ref_multiplier[g] + alt_i[m[0]] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[m[0]], mismap1[m[0]], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[m[0]], 1 - mismap2[m[0]]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          for(std::size_t j1=0; j1<n_p; ++j1){
            if(std::isinf(p_emit1[j1])){
              score_j[j1] = neg_inf;
            } else {
              
              std::size_t valid_j1;
              for(std::size_t j=0;j<valid_p_indices1.size();++j){
                valid_j1 = valid_p_indices1[j];
                if(j1 == valid_j1){
                  valid_j1 = j;
                  break;
                }
              }
              for(std::size_t k=0; k<n_h; ++k){
                hap_prob = prob_i[possiblehap[j2 * n_h + k]];
                log10_safe(hap_prob);
                col_in = valid_j1 * n_h + k;
                score_ijk[k] = in_i[col_in] + hap_prob;
              }
              score_j[j1] += logsum(score_ijk);
            }
          }
        }
        for(std::size_t j1=0; j1<n_p; ++j1){
          score_j[j1] = p_emit1[j1] + score_j[j1];
        }
        
        max_j = get_max_int(score_j);
        f_path_m[j2] = max_j;
        
        std::size_t max_valid_index;
        std::size_t valid_j1;
        for(std::size_t j=0;j<valid_p_indices1.size();++j){
          valid_j1 = valid_p_indices1[j];
          if(max_j == valid_j1){
            max_valid_index = j;
          }
        }
        
        std::size_t j2_valid_index;
        std::size_t valid_j2;
        for(std::size_t j=0;j<valid_p_indices2.size();++j){
          valid_j2 = valid_p_indices2[j];
          if(j2 == valid_j2){
            j2_valid_index = j;
          }
        }
        
        std::size_t out_i;
        
        for(std::size_t sample_i=0; sample_i<n_o; ++sample_i){
          RMatrix<double>::Row vit_i = vit_score.row(sample_i);
          RMatrix<double>::Row in_i = in_score.row(sample_i);
          
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[m[0]], eseq[1]};
          alt_multiplier = {eseq[1], w2[m[0]], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[m[0]] * ref_multiplier[g] + alt_i[m[0]] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[m[0]], mismap1[m[0]], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[m[0]], 1 - mismap2[m[0]]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          for(std::size_t k=0; k<n_h; ++k){
            col_i = j2 * n_h + k;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            col_in = max_valid_index * n_h + k;
            out_i = j2_valid_index * n_h + k;
            vit_i[out_i] = in_i[col_in] + hap_prob;
          }
        }
      }
    }
  }
};

// Calculate alpha values in the forward algorithm
void last_vit_founder(IntegerVector f_seq,
                      NumericMatrix vit_score,
                      NumericVector p_emit,
                      IntegerVector dim,
                      IntegerVector valid_p_indices){
  std::size_t n_o = dim[2];
  std::size_t n_p = dim[3];
  std::size_t n_h = dim[4];
  std::size_t m = dim[0]-1;
  std::size_t vit_i;
  double neg_inf = -std::numeric_limits<double>::infinity();
  
  // Calculate emit, alpha, and gamma
  IntegerMatrix tmp_argmax(n_o, valid_p_indices.size());
  std::vector<double> score_ijk(n_h);
  std::vector<double> score_j(n_p);
  
  for(std::size_t i=0; i<n_o; ++i){
    for(std::size_t j1=0; j1<n_p; ++j1){
      std::size_t valid_j1;
      for(R_xlen_t j=0;j<valid_p_indices.size();++j){
        valid_j1 = valid_p_indices[j];
        if(j1 == valid_j1){
          valid_j1 = j;
          break;
        }
      }
      
      if(std::isinf(p_emit[j1])){
        score_j[j1] = neg_inf;
        
      } else {
        for(std::size_t k=0; k<n_h; ++k){
          vit_i = valid_j1 * n_h + k;
          score_ijk[k] = vit_score(i, vit_i);
        }
        score_j[j1] += logsum(score_ijk);
      }
    }
  }
  for(std::size_t j=0; j<n_p; ++j){
    score_j[j] = p_emit[j] + score_j[j];
  }
  std::size_t max_j = get_max_int(score_j);
  f_seq[m] = max_j;
}

// Find the best state sequences backwardly
void backtrack(IntegerMatrix f_path,
               IntegerVector f_seq,
               IntegerVector dim){
  std::size_t f_prev;
  std::size_t n_m = dim[0];
  
  for(std::size_t m=n_m-1; m>0; --m){
    if(m % 10 == 9){
      Rcpp::Rcout << "\r" << "Backtracking best genotype sequences at marker#: " << m+1 << std::string(70, ' ');
    }
    f_prev = f_seq[m];
    f_seq[m-1] = f_path(m , f_prev);
  }
  Rcpp::Rcout << "\r" << "Backtracking best genotype sequences: Dene!" << std::string(70, ' ');
}

// Functions for the Viterbi algorithm For OFFSPRING
struct ParVitOffspring : public Worker {
  
  const RMatrix<int> o_seq;
  const RVector<int> iter_sample;
  const RMatrix<double> ref;
  const RMatrix<double> alt;
  const RVector<double> eseq;
  const RVector<double> w1;
  const RVector<double> w2;
  const RVector<double> mismap1;
  const RVector<double> mismap2;
  const RVector<int> possiblehap;
  const RVector<double> init_prob;
  const RMatrix<double> trans_prob;
  const RVector<int> dim;
  const RVector<int> f_seq;
  
  
  ParVitOffspring(IntegerMatrix o_seq,
                  const LogicalVector iter_sample,
                  const NumericMatrix ref,
                  const NumericMatrix alt,
                  const NumericVector eseq,
                  const NumericVector w1,
                  const NumericVector w2,
                  const NumericVector mismap1,
                  const NumericVector mismap2,
                  const IntegerVector possiblehap,
                  const NumericVector init_prob,
                  const NumericMatrix trans_prob,
                  const IntegerVector dim,
                  const IntegerVector f_seq)
    : o_seq(o_seq),
      iter_sample(iter_sample),
      ref(ref),
      alt(alt),
      eseq(eseq),
      w1(w1),
      w2(w2),
      mismap1(mismap1),
      mismap2(mismap2),
      possiblehap(possiblehap),
      init_prob(init_prob),
      trans_prob(trans_prob),
      dim(dim),
      f_seq(f_seq) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_sample.begin() + begin; i<iter_sample.begin() + end; ++i){
      std::size_t sample_i = std::distance(iter_sample.begin(), i);
      RMatrix<int>::Column o_seq_i = o_seq.column(sample_i);
      RMatrix<double>::Row ref_i = ref.row(sample_i);
      RMatrix<double>::Row alt_i = alt.row(sample_i);
      std::size_t n_m = dim[0];
      std::size_t n_h = dim[4];
      std::vector<std::vector<unsigned short>> o_path(n_m,
                                                      std::vector<unsigned short>(n_h));
      std::size_t col_i;
      double hap_prob = 0.0;
      std::vector<double> vit(n_h);
      std::vector<double> prob_i(3);
      double sum_v1 = 0.0;
      double sum_v2 = 0.0;
      double sum_v3 = 0.0;
      double sum_v = 0.0;
      double trans_kk;
      std::size_t max_i;
      std::vector<double> score_jkk(n_h);
      std::vector<double> max_scores_jk(n_h);
      std::size_t o_prev;
      
      // Viterbi path
      for(std::size_t m=0; m<n_m; ++m){
        // Calculate genotype probabilies
        std::vector<double> ref_multiplier = {eseq[0], w1[m], eseq[1]};
        std::vector<double> alt_multiplier = {eseq[1], w2[m], eseq[0]};
        for(int g=0; g<3;++g){
          prob_i[g] = ref_i[m]* ref_multiplier[g] + alt_i[m] * alt_multiplier[g];
        }
        lognorm_vec(prob_i);
        for(int g=0; g<3;++g){
          prob_i[g] = pow10(prob_i[g]);
        }
        
        // Calculate mismap accounted genotype probabilities
        std::vector<double> v1 = {1 - mismap1[m], mismap1[m], 0};
        std::vector<double> v2 = {0, 1, 0};
        std::vector<double> v3 = {0, mismap2[m], 1 - mismap2[m]};
        
        sum_v1 = 0.0;
        sum_v2 = 0.0;
        sum_v3 = 0.0;
        for(std::size_t g=0; g<3;++g){
          sum_v1 += v1[g] * prob_i[g];
          sum_v2 += v2[g] * prob_i[g];
          sum_v3 += v3[g] * prob_i[g];
        }
        sum_v = sum_v1 + sum_v2 + sum_v3;
        prob_i[0] = sum_v1 / sum_v;
        prob_i[1] = sum_v2 / sum_v;
        prob_i[2] = sum_v3 / sum_v;
        
        std::size_t f_geno = f_seq[m];
        std::size_t trans_prob_col;
        
        if(m == 0){
          for(std::size_t k=0; k<n_h; ++k){
            col_i = f_geno * n_h + k;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            vit[k] = hap_prob + init_prob[k];
          }
        } else {
          
          for(std::size_t k2=0; k2<n_h; ++k2){
            trans_prob_col = (m-1)*n_h + k2;
            RMatrix<double>::Column trans_prob_k = trans_prob.column(trans_prob_col);
            for(std::size_t k1=0; k1<n_h; ++k1){
              trans_kk = trans_prob_k[k1];
              score_jkk[k1] = vit[k1] + trans_kk;
            }
            max_i = get_max_int(score_jkk);
            o_path[m][k2] = max_i;
            max_scores_jk[k2] = score_jkk[max_i];
          }
          for(std::size_t k2=0; k2<n_h; ++k2){
            col_i = f_geno * n_h + k2;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            vit[k2] = max_scores_jk[k2] + hap_prob;
          }
        }
        if(m == n_m - 1){
          o_seq_i[m] = get_max_int(vit);
        }
      }
      
      // Backtracking
      for(std::size_t m=n_m-1; m>0; --m){
        o_prev = o_seq_i[m];
        o_seq_i[m-1] = o_path[m][o_prev];
      }
    }
  }
};


// Solve the HMM
// [[Rcpp::export]]
List run_viterbi2(NumericMatrix p_ref,
                  NumericMatrix p_alt,
                  NumericMatrix ref,
                  NumericMatrix alt,
                  NumericVector eseq_in,
                  NumericVector bias,
                  NumericMatrix mismap,
                  NumericMatrix trans_prob,
                  NumericVector init_prob,
                  int & n_p,
                  int & n_h,
                  int & n_o,
                  int & n_f,
                  int & n_m,
                  IntegerVector possiblehap,
                  IntegerVector possiblegeno,
                  IntegerVector p_geno_fix
){
  // Initialize arrays to store output, alpha values, emittion probs, and beta values.
  double neg_inf = -std::numeric_limits<double>::infinity();
  IntegerVector f_seq(n_m);
  IntegerMatrix f_path(n_m, n_p);
  IntegerMatrix o_seq(n_m, n_o);
  
  // Convert values to ones used here.
  IntegerVector dim = {n_m, n_f, n_o, n_p, n_h};
  NumericVector w1(n_m);
  NumericVector w2(n_m);
  NumericVector eseq(2);
  NumericVector mismap1 = mismap( _ , 0 );
  NumericVector mismap2 = mismap( _ , 1 );
  eseq = clone(eseq_in);
  
  for(R_xlen_t i=0; i<eseq.size(); ++i){
    log10_safe(eseq[i]);
  }
  w1 = clone(bias);
  w2 = clone(bias);
  w2 = 1 - w2;
  for(R_xlen_t i=0; i<w1.size(); ++i){
    log10_safe(w1[i]);
    log10_safe(w2[i]);
  }
  
  LogicalVector iter_sample(n_o);
  LogicalVector iter_p_pat(n_p);
  NumericVector p_emit1;
  NumericVector p_emit2;
  
  // Initialize Viterbi trellis.
  int m = 0;
  p_emit1 = calcPemit(p_ref,
                      p_alt,
                      eseq,
                      w1,
                      w2,
                      mismap1,
                      mismap2,
                      possiblegeno,
                      m,
                      n_f,
                      n_p);
  
  int int_fix_p = p_geno_fix[0];
  if(int_fix_p >= 0){
    R_xlen_t fix_p = p_geno_fix[0];
    for(R_xlen_t p=0; p<p_emit1.size(); ++p){
      if(p == fix_p){
        p_emit1[p] = 0;
      } else {
        p_emit1[p] = neg_inf;
      }
    }
  }
  
  IntegerVector valid_p_indices1;
  for(R_xlen_t j=0; j<p_emit1.size(); ++j){
    if(!std::isinf(p_emit1[j])){
      valid_p_indices1.push_back(j);
    }
  }
  std::size_t valid_size = valid_p_indices1.size();
  NumericMatrix vit_score(n_o, valid_size * n_h);
  ParInitVit init_vit(vit_score,
                      iter_sample,
                      ref,
                      alt,
                      eseq,
                      w1,
                      w2,
                      mismap1,
                      mismap2,
                      possiblehap,
                      init_prob,
                      dim,
                      valid_p_indices1);
  parallelFor(0, iter_sample.length(), init_vit);
  NumericMatrix in_score;
  in_score = clone(vit_score);
  
  for(int m=1; m<n_m; ++m){
    
    if(m % 10 == 9){
      Rcpp::Rcout << "\r" << "Forward founder genotype probability calculation at marker#: " << m+1 << std::string(70, ' ');
    }
    NumericMatrix trans_prob_m = trans_prob( _ , Range( (m-1)*n_h , (m-1)*n_h + n_h - 1 ) );
    
    ParCalcVitFounder calc_vit(in_score,
                               iter_sample,
                               trans_prob_m,
                               dim,
                               valid_p_indices1);
    parallelFor(0, iter_sample.length(), calc_vit);
    
    p_emit2 = calcPemit(p_ref,
                        p_alt,
                        eseq,
                        w1,
                        w2,
                        mismap1,
                        mismap2,
                        possiblegeno,
                        m,
                        n_f,
                        n_p);
    
    R_xlen_t fix_p_len = p_geno_fix.size();
    if(fix_p_len > m){
      R_xlen_t fix_p = p_geno_fix[m];
      for(R_xlen_t p=0; p<p_emit2.size(); ++p){
        if(p == fix_p){
          p_emit2[p] = 0;
        } else {
          p_emit2[p] = neg_inf;
        }
      }
    };
    
    IntegerVector valid_p_indices2;
    for(R_xlen_t j=0; j<p_emit2.size(); ++j){
      if(!std::isinf(p_emit2[j])){
        valid_p_indices2.push_back(j);
      }
    }
    
    std::size_t valid_size = valid_p_indices2.size();
    NumericMatrix vit_score(n_o, valid_size * n_h);
    IntegerVector in_m = {m};
    ParCalcPathFounder calc_path(f_path,
                                 vit_score,
                                 in_score,
                                 iter_p_pat,
                                 ref,
                                 alt,
                                 eseq,
                                 w1,
                                 w2,
                                 mismap1,
                                 mismap2,
                                 possiblehap,
                                 dim,
                                 p_emit1,
                                 p_emit2,
                                 valid_p_indices1,
                                 valid_p_indices2,
                                 in_m);
    
    parallelFor(0, iter_p_pat.length(), calc_path);
    p_emit1 = clone(p_emit2);
    valid_p_indices1 = clone(valid_p_indices2);
    in_score = clone(vit_score);
    
    if(m==n_m-1){
      
      last_vit_founder(f_seq,
                       in_score,
                       p_emit1,
                       dim,
                       valid_p_indices1);
    }
  }
  Rcpp::Rcout << "\r" << "Forward founder genotype probability calculation: Dene!" << std::string(70, ' ');
  
  backtrack(f_path,
            f_seq,
            dim);
  
  
  ParVitOffspring vit_offspring(o_seq,
                                iter_sample,
                                ref,
                                alt,
                                eseq,
                                w1,
                                w2,
                                mismap1,
                                mismap2,
                                possiblehap,
                                init_prob,
                                trans_prob,
                                dim,
                                f_seq);
  parallelFor(0, iter_sample.length(), vit_offspring);
  
  Rcpp::Rcout << "\r" << std::string(70, ' ');
  List out_list = List::create(_["p_geno"] = f_seq, _["best_seq"] = o_seq);
  return out_list;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions for the forward-backward algorithm

struct ParFB : public Worker {
  
  RMatrix<double> gamma;
  const RVector<int> iter_sample;
  const RMatrix<double> ref;
  const RMatrix<double> alt;
  const RVector<double> eseq;
  const RVector<double> w1;
  const RVector<double> w2;
  const RVector<double> mismap1;
  const RVector<double> mismap2;
  const RVector<int> possiblehap;
  const RVector<double> init_prob;
  const RMatrix<double> trans_prob;
  const RVector<int> dim;
  const RVector<int> p_geno;
  
  ParFB(NumericMatrix gamma,
        const LogicalVector iter_sample,
        const NumericMatrix ref,
        const NumericMatrix alt,
        const NumericVector eseq,
        const NumericVector w1,
        const NumericVector w2,
        const NumericVector mismap1,
        const NumericVector mismap2,
        const IntegerVector possiblehap,
        const NumericVector init_prob,
        const NumericMatrix trans_prob,
        const IntegerVector dim,
        const IntegerVector p_geno)
    : gamma(gamma),
      iter_sample(iter_sample),
      ref(ref),
      alt(alt),
      eseq(eseq),
      w1(w1),
      w2(w2),
      mismap1(mismap1),
      mismap2(mismap2),
      possiblehap(possiblehap),
      init_prob(init_prob),
      trans_prob(trans_prob),
      dim(dim),
      p_geno(p_geno) {}
  
  void operator()(std::size_t begin, std::size_t end) {
    
    for(RVector<int>::const_iterator i=iter_sample.begin() + begin; i<iter_sample.begin() + end; ++i){
      std::size_t sample_i = std::distance(iter_sample.begin(), i);
      RMatrix<double>::Row gamma_i = gamma.row(sample_i);
      
      std::size_t n_m = dim[0];
      std::size_t n_h = dim[2];
      
      std::vector<std::vector<double>> alpha(n_m,
                                             std::vector<double>(n_h));
      std::vector<std::vector<double>> emit(n_m,
                                            std::vector<double>(n_h));
      std::vector<double> beta(n_h, 1);
      
      std::vector<double> ref_multiplier(3);
      std::vector<double> alt_multiplier(3);
      std::vector<double> v1;
      std::vector<double> v2;
      std::vector<double> v3;
      vector<double> prob_i(3);
      vector<double> score_k(n_h);
      double sum_v;
      double hap_prob;
      double trans_kk;
      double sum_k;
      std::size_t col_i;
      std::size_t j;
      
      for(std::size_t m=0; m<n_m; ++m){
        if(m == 0){
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[0], eseq[1]};
          alt_multiplier = {eseq[1], w2[0], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[0]* ref_multiplier[g] + alt_i[0] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[0], mismap1[0], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[0], 1 - mismap2[0]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          j = p_geno[0];
          for(std::size_t k=0; k<n_h; ++k){
            col_i = j * n_h + k;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            alpha[m][k] = hap_prob + init_prob[k];
          }
          
        } else {
          // Calculate genotype probabilies
          ref_multiplier = {eseq[0], w1[m], eseq[1]};
          alt_multiplier = {eseq[1], w2[m], eseq[0]};
          RMatrix<double>::Row ref_i = ref.row(sample_i);
          RMatrix<double>::Row alt_i = alt.row(sample_i);
          for(int g=0; g<3;++g){
            prob_i[g] = ref_i[m]* ref_multiplier[g] + alt_i[m] * alt_multiplier[g];
          }
          lognorm_vec(prob_i);
          for(int g=0; g<3;++g){
            prob_i[g] = pow10(prob_i[g]);
          }
          
          // Calculate mismap accounted genotype probabilities
          v1 = {1 - mismap1[m], mismap1[m], 0};
          v2 = {0, 1, 0};
          v3 = {0, mismap2[m], 1 - mismap2[m]};
          double sum_v1 = 0.0;
          double sum_v2 = 0.0;
          double sum_v3 = 0.0;
          for(int g=0; g<3;++g){
            sum_v1 += v1[g] * prob_i[g];
            sum_v2 += v2[g] * prob_i[g];
            sum_v3 += v3[g] * prob_i[g];
          }
          sum_v = sum_v1 + sum_v2 + sum_v3;
          prob_i[0] = sum_v1 / sum_v;
          prob_i[1] = sum_v2 / sum_v;
          prob_i[2] = sum_v3 / sum_v;
          
          std::size_t j = p_geno[m];
          
          for(std::size_t k2=0; k2<n_h; ++k2){
            RMatrix<double>::Column trans_prob_k = trans_prob.column((m-1)*n_h + k2);
            for(std::size_t k1=0; k1<n_h; ++k1){
              trans_kk = trans_prob_k[k1];
              score_k.at(k1) = alpha[m-1][k1] + trans_kk;
            }
            sum_k = logsum(score_k);
            col_i = j * n_h + k2;
            hap_prob = prob_i[possiblehap[col_i]];
            log10_safe(hap_prob);
            emit[m][k2] = hap_prob;
            alpha[m][k2] = hap_prob + sum_k;
          }
        }
      }
      
      std::vector<double> gamma_i1;
      std::vector<double> gamma_i2;
      std::vector<double> gamma_i3;
      std::vector<double> gamma_i_tmp(3);
      for(std::size_t k=0; k<n_h; ++k){
        std::size_t j = p_geno[n_m-1];
        col_i = j * n_h + k;
        if(possiblehap[col_i] == 0){
          gamma_i1.push_back(beta[k] + alpha[n_m-1][k]);
        }
        if(possiblehap[col_i] == 1){
          gamma_i2.push_back(beta[k] + alpha[n_m-1][k]);
        }
        if(possiblehap[col_i] == 2){
          gamma_i3.push_back(beta[k] + alpha[n_m-1][k]);
        }
      }
      gamma_i_tmp[0] = logsum(gamma_i1);
      gamma_i_tmp[1] = logsum(gamma_i2);
      gamma_i_tmp[2] = logsum(gamma_i3);
      lognorm_vec(gamma_i_tmp);
      for(int g=0; g<3;++g){
        gamma_i[(n_m-1) * 3 + g] = gamma_i_tmp[g];
      }
      
      for(std::size_t m=n_m-1; m>0; --m){
        std::vector<double> gamma_i1;
        std::vector<double> gamma_i2;
        std::vector<double> gamma_i3;
        std::vector<double> gamma_i_tmp(3);
        
        for(std::size_t k1=0; k1<n_h; ++k1){
          RMatrix<double>::Row trans_prob_k = trans_prob.row(k1);
          for(std::size_t k2=0; k2<n_h; ++k2){
            trans_kk = trans_prob_k[(m-1)*n_h + k2];
            score_k[k2] = emit[m][k2] + beta[k2] + trans_kk;
          }
          sum_k = logsum(score_k);
          beta[k1] = sum_k;
          
          j = p_geno[m-1];
          col_i = j * n_h + k1;
          if(possiblehap[col_i] == 0){
            gamma_i1.push_back(sum_k + alpha[m-1][k1]);
          }
          if(possiblehap[col_i] == 1){
            gamma_i2.push_back(sum_k + alpha[m-1][k1]);
          }
          if(possiblehap[col_i] == 2){
            gamma_i3.push_back(sum_k + alpha[m-1][k1]);
          }
        }
        gamma_i_tmp[0] = logsum(gamma_i1);
        gamma_i_tmp[1] = logsum(gamma_i2);
        gamma_i_tmp[2] = logsum(gamma_i3);
        lognorm_vec(gamma_i_tmp);
        for(int g=0; g<3;++g){
          gamma_i[(m-1) * 3 + g] = gamma_i_tmp[g];
        }
      }
    }
  }
};

// Solve the HMM for offspring
// [[Rcpp::export]]
NumericMatrix run_fb(NumericMatrix ref,
                     NumericMatrix alt,
                     NumericVector eseq_in,
                     NumericVector bias,
                     NumericMatrix mismap,
                     IntegerVector possiblehap,
                     NumericMatrix trans_prob,
                     NumericVector init_prob,
                     int & n_h,
                     int & n_o,
                     int & n_m,
                     IntegerVector p_geno
){
  
  
  // Initialize arrays to store output, alpha values, emittion probs, and beta values.
  NumericMatrix gamma(n_o,  n_m * 3);
  
  // Convert values to ones used here.
  IntegerVector dim = {n_m, n_o, n_h};
  NumericVector w1(n_m);
  NumericVector w2(n_m);
  NumericVector eseq(2);
  NumericVector mismap1 = mismap( _ , 0 );
  NumericVector mismap2 = mismap( _ , 1 );
  eseq = clone(eseq_in);
  
  for(R_xlen_t i=0; i<eseq.size(); ++i){
    log10_safe(eseq[i]);
  }
  w1 = clone(bias);
  w2 = clone(bias);
  w2 = 1 - w2;
  for(R_xlen_t i=0; i<w1.size(); ++i){
    log10_safe(w1[i]);
    log10_safe(w2[i]);
  }
  
  LogicalVector iter_sample(n_o);
  
  ParFB calc_fb(gamma,
                iter_sample,
                ref,
                alt,
                eseq,
                w1,
                w2,
                mismap1,
                mismap2,
                possiblehap,
                init_prob,
                trans_prob,
                dim,
                p_geno);
  
  parallelFor(0, iter_sample.length(), calc_fb);
  
  gamma.attr("dim") = Dimension(n_o, 3, n_m);
  return gamma;
}

