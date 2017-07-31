function [n,m,X] = init_encoder(mess,nb_sortie,nb_etage,Gen)

  // Cette fonction est une simple fonction de test de validit� des
  // param�tres.

  n = nb_sortie;
  m = nb_etage;
  X = Gen;

  if size(X) ~= [n m] 
    error('Erreur de dimension des codes g�n�rateurs');
  end

  if (sum(X>1 | X<0)>0) then
    error ('Le code g�n�rateur doit �tre en binaire');
  end

  if (sum(mess>1 | mess<0)>0) then
    error ('Le message doit �tre en binaire');
  end

endfunction

//================================================================//

function code = convol_encoder(mess,nb_sortie,nb_etage,Gen)

// Cette fonction permet de coder un message en utilisant le codeur
// convolutionnel. Il faut pour cela passer en param�tre le message �
// coder en binaire, puis le nombre de sorties (n) du codeur
// convolutionnel, le nombre d'�tages du registre (m) du codeur
// convolutionnel, et enfin la matrice contenant les polynomes
// g�n�rateurs en binaire (chaque ligne correspondant � un polynome).

// Sch�ma d'un codeur
//
//
//
//
//          entr�e         _ _ _     _ _         sorties
//				1    ---->|_|_|_|...|_|_|----> n
//				    		  m �tages      
// Le codeur poss�de n sommateurs de type ou exclusif qui donnent les
// n bits de sorties.
//

  // Test de validit� des param�tres
  [n,m,X] = init_encoder(mess,nb_sortie,nb_etage,Gen);

  l = length(mess);
  code = zeros(1,(l+m-1)*n);
  W = zeros(n,l+m-1);

  for i = 1:n
    W(i,:) = modulo(round(convol(mess,X(i,:))),2);
  end
  code = W(:)';

endfunction

