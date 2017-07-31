function b = db(x,p) 

    //codage du d�cimal vers le binaire sur p bits
    //on passera en param�tre le nombre en base 10(x) que l'on veut coder 
    //et l'entier(p) d�signant le nombre de bits souhait�

    b = zeros(1,p);
    if x>=2^p then error('dimension');
    end
    for i = 1:p
    d = x-2^(p-i);
    b(i) = (d>=0)*1;
    x = x-2^(p-i)*b(i);
    end

endfunction


function table = Table(n,m,x)

    //Cette fonction g�re la cr�ation de la table contenant les bits de sorties possibles
    //elle retourne une matrice de dimension (2^(m-1)=le nombre d'�tat,2*n=2*nb de sorties du codeur)
    //il faut placer en param�tre le nombre de sorties du codeur,le nombre d'�tage du registre et les polynomes
    //g�n�rateurs en binaire sous forme de matrice: chaque ligne correspondant � un polynome

    table = zeros(2^(m-1),2*n);
    E = zeros(2^(m-1),m-1);

    for i = 1:size(table,'r')
    E(i,:) = db(i-1,m-1);
    end
    //E contient tous les �tats possibles

    for i = 1:size(table,'r'),for j = 1:n
    table(i,j) = modulo([0 E(i,:)]*x(j,:)',2);
    table(i,n+j) = modulo([1 E(i,:)]*x(j,:)',2);
    end,end

endfunction


//algorithme de d�codage (Viterbi)
function mess = convol_decoder(code,n,m,x)

    //cette fonction permet de d�coder un code en suivant l'algorithme de Viterbi
    // il faut pour cela passer en param�tre le mot code en binaire
    // puis le nombre de sorties(n) du codeur convolutionnel
    // le nombre d'�tage du registre(m) du codeur convolutionnel
    // et enfin la matrice contenant les polynomes g�n�rateurs en binaire(chaque ligne correspondant � un polynome)

    // test de validit� des param�tres
    init_encoder(code,n,m,x);

    //d�claration

    compteur = 1;
    f = 6*m;//fen�tre de d�codage: 6 fois la longueur de contrainte
    L = floor(length(code)/n);// L est la longueur du message original avant le codeur

    d = zeros(2^(m-1),2);
    // 2^(m-1) est le nombre d'etats
    // la matrice d va stocker � chaque fois les 2^m poids possibles 

    Dist = zeros(2^(m-1),f+1);
    Dist(:,1) = [0;10000*ones(2^(m-1)-1,1)];
    // la matrice Dist va stocker dans chaque colonne le poids des diff�rents �tats ;
    // ces poids sont retenus � chaque tour de boucle par l'�valuation de la matrice d

    P = zeros(2^(m-1),f);
    // la matrice P va contenir le num�ro de l'�tat pr�c�dent qui a men� � l'�tat actuel
    // dans la matrice(la position dans une des colonnes de P donne l'�tat actuel),
    // l'�tat(00) est not�(1) et ainsi de suite

    M = zeros(2^(m-1),f-1);
    // M est une matrice "interm�diaire" n�cessaire � la r��valuation de P:
    // �limination des chemins qui "s'arr�tent"

    mess = zeros(1,L);

    // construction de matrices utiles pour les calculs de l'algorithme

    Jd = [zeros(f+1,1) eye(f+1,f+1)];
    Jd = Jd(:,1:f+1);
    Jp = [zeros(f,1) eye(f,f)];
    Jp = Jp(:,1:f);
    // Jd et Jp sont les matrices identit�s d�cal�es d'une colonne vers la droite

    I = 1:2^(m-1);
    I = matrix(I,2,2^(m-2));
    I = [I I];
    // par exemple pour m=3 on a I=[1 3 1 3;2 4 2 4] et pour m=4 I=[1 3 5 7 1 3 5 7;2 4 6 8 2 4 6 8]
    // cette matrice sert pour retrouver l'�tat pr�c�dent et remplir P


    //cr�ation de la table contenant les bits de sorties possibles
    table_sortie = Table(n,m,x);

    code_etat = [zeros(1,2^(m-2)) ones(1,2^(m-2))]


    code = [code zeros(1,f*L-length(code))];


    t = size(table_sortie,'r');


    for i = 1:f

    // on compare les bits du code avec les bits de la table
    for j = 1:2,for k = 1:t,
    d(k,j) = sum(modulo(round(code((i-1)*n+1:i*n)+table_sortie(k,(j-1)*n+1:j*n)),2))+Dist(k,i);
end
end

// pour chaque �tat on garde le chemin arrivant le plus petit
dbis = matrix(d,2,t);
[Min,ind] = min(dbis,'r');
Dist(:,i+1) = Min';

// P contient le num�ro de l'�tat pr�cedent qui a permis d'arriver � l'�tat pr�sent
for l = 1:size(Dist,'r')
P(l,i) = I(ind(l),l);
end,


    end

    //on �limine les chemins qui "s'arr�tent" au fur et � mesure
    for i = 1:f-1,for j = 1:size(P,'r')
    if P(j,f-i+1) ~= 0 then
    M(P(j,f-i+1),f-i) = 1;
    end
    end,
    P(:,f-i) = P(:,f-i).*M(:,f-i);
end


z = 1;
Z = zeros(z,2);
// on regarde les positions restantes au d�but de la fen�tre (6 fois la longueur de contrainte)
for j = 1:size(P,'r')
if P(j,1)~=0 then
Z(z,:) = [j Dist(j,2)];
z = z+1;
end
end

// on garde celui de poids le plus faible
[Min,ind] = min(Z(:,2));
p = ind;

// suivant sa position on en d�duit le bit du message qui avait �t� cod�
mess(compteur) = code_etat(Z(p,1));


winId  =  waitbar('D�codage s�quence');
ref = round(L/100);

while compteur<L,

      // On reste dans la boucle tant que le message n'est pas complet

      if modulo(compteur,ref) == 0
      waitbar(round(compteur/L*100)/100,winId);
      end

      M = zeros(2^(m-1),f-1);

      // on d�cale les matrices Dist et P d'une colonne vers la gauche
      Dist = Dist*Jd';
      P = P*Jp';

      // puis on calcule les derni�res colonnes de ces 2 matrices

      for j = 1:2,for k = 1:t
      d(k,j) = sum(modulo(round(code((f+compteur-1)*n+1:(f+compteur)*n)+table_sortie(k,(j-1)*n+1:j*n)),2))+Dist(k,f);
      end
      end


      dbis  = matrix(d,2,t);
      [Min,ind] = min(dbis,'r');
      Dist(:,f+1) = Min';

      for l = 1:size(Dist,'r')
      P(l,f) = I(ind(l),l);
      end,

      // on �limine les chemins qui "s'arr�tent"
      for i = 1:f-1,for j = 1:size(P,'r')
      if P(j,f-i+1)~=0 then
      M(P(j,f-i+1),f-i) = 1;
      end
      end,
      P(:,f-i) = P(:,f-i).*M(:,f-i);
end

// on regarde les positions restantes au d�but de la fen�tre (6 fois la longueur de contrainte)
z = 1;
Z = zeros(z,2);
for j = 1:size(P,'r')
if P(j,1)~=0 then
Z(z,:) = [j Dist(j,2)];
z = z+1;
end
end
// on garde celui de poids le plus faible
[Min,ind] = min(Z(:,2));
p = ind;

// suivant sa position on en d�duit le bit du message qui avait �t� cod�
mess(compteur+1) = code_etat(Z(p,1));

compteur = compteur+1;// on incr�mente le compteur de passage dans la boucle

end

winclose(winId);

endfunction

