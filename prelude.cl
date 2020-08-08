
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;      Lispy Standard Prelude      ;;;
;;; http://www.buildyourownlisp.com/ ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;
;;; Atoms
;;;

(def {nil} {})
(def {true} 1)
(def {false} 0)

;;;
;;; Functional functions
;;;

; Function definitions
(def {fun} (\ {args body} {
    def (head args) (\ (tail args) body)}))

; Open new scope
(fun {let body} {
    ((\ {_} body) ())})

; Unpack list to function
(fun {unpack f lst} {
    eval (join (list f) lst)})

; Pack list to function
(fun {pack f & lst} {f lst})

; Curried and uncurried calling
(def {curry} unpack)
(def {uncurry} pack)

; Perform several things in sequence
(fun {do & lst} {
    if (== lst nil)
        {nil}
        {last lst}})

;;;
;;; Logical functions
;;;

(fun {not x}   {- 1 x})
(fun {or  x y} {+ x y})
(fun {and x y} {* x y})

;;;
;;; Miscelaneous functions
;;;

(fun {flip f a b} {f b a})
(fun {comp f g x} {f (g x)})
(fun {ghost & xs} {eval xs})

; Fibonacci
(fun {fib n} {
    select
        {(== n 0) 0}
        {(== n 1) 1}
        {otherwise (+ (fib (- n 1)) (fib (- n 2)))}})

;;;
;;; Numeric functions
;;;

; Minimum of arguments
(fun {min & xs} {
    if (== (tail xs) nil)
        {fst xs}
        {do
            (= {rest} (unpack min (tail xs)))
            (= {item} (fst xs))
            (if (< item rest)
                {item}
                {rest})}})

; Maximum of arguments
(fun {max & xs} {
    if (== (tail xs) nil)
        {fst xs}
        {do
            (= {rest} (unpack max (tail xs)))
            (= {item} (fst xs))
            (if (> item rest)
                {item}
                {rest})}})

;;;
;;; Conditional functions
;;;

(fun {select & cs} {
    if (== cs nil)
        {error "No selection found"}
        {if (fst (fst cs))
            {snd (fst cs)}
            {unpack select (tail cs)}}})

(fun {case x & cs} {
    if (== cs nil)
        {error "No case found"}
        {if (== x (fst (fst cs)))
            {snd (fst cs)}
            {unpack case (join (list x) (tail cs))}}})

(def {otherwise} true)

;;;
;;; List functions
;;;

; First, second, and third item in list
(fun {fst lst} {eval (head lst)})
(fun {snd lst} {eval (head (tail lst))})
(fun {trd lst} {eval (head (tail (tail lst)))})

; List length
(fun {len lst} {
    if (== lst nil)
        {0}
        {+ 1 (len (tail lst))}})

; N-th item in list
(fun {nth n lst} {
    if (== n 0)
        {fst lst}
        {nth (- n 1) (tail lst)}})

; Last item in list
(fun {last lst} {
    nth (- (len lst) 1) lst})

; Apply function to list
(fun {map f lst} {
    if (== lst nil)
        {nil}
        {join
            (list (f (fst lst)))
            (map f (tail lst))}})

; Apply filter to list
(fun {filter f lst} {
    if (== lst nil)
        {nil}
        {join
            (if (f (fst lst))
                {head lst}
                {nil})
            (filter f (tail lst))}})

; Return all of list but last element
(fun {init lst} {
    if (== (tail lst) nil)
        {nil}
        {join (head lst) (init (tail lst))}})

; Reverse list
(fun {reverse l} {
    if (== lst nil)
        {nil}
        {join (reverse (tail lst)) (head lst)}})

; Fold left
(fun {foldl f z lst} {
    if (== lst nil)
        {z}
        {foldl f (f z (fst lst)) (tail lst)}})

; Fold right
(fun {foldr f z lst} {
    if (== lst nil)
        {z}
        {f (fst lst) (foldr f z (tail lst))}})

(fun {sum lst} {foldl + 0 lst})
(fun {product lst} {foldl * 1 lst})

; Take n items
(fun {take n lst} {
    if (== n 0)
        {nil}
        {join
            (head lst)
            (take (- n 1) (tail lst))}})

; Drop n items
(fun {drop n lst} {
    if (== n 0)
        {lst}
        {drop (- n 1) (tail lst)}})

; Split at n
(fun {split n lst} {list (take n lst) (drop n lst)})

; Take while
(fun {take-while f l} {
  if (not (unpack f (head l)))
    {nil}
    {join (head l) (take-while f (tail l))}})

; Drop while
(fun {drop-while f l} {
  if (not (unpack f (head l)))
    {l}
    {drop-while f (tail l)}})

; Element of list
(fun {elem x lst} {
    if (== lst nil)
        {false}
        {if (== x (fst lst))
            {true}
            {elem x (tail lst)}}})

; Find element in list of pairs
(fun {lookup x lst} {
    if (== lst nil)
        {error "No element found"}
        {do
            (= {key} (fst (fst lst)))
            (= {val} (snd (fst lst)))
            (if (== key x)
                {val}
                {lookup x (tail lst)})}})

; Zip two lists together into a list of pairs
(fun {zip x y} {
    if (or (== x nil) (== y nil))
        {nil}
        {join
            (list (join (head x) (head y)))
            (zip (tail x) (tail y))}})

; Unzip a list of pairs into two lists
(fun {unzip lst} {
    if (== lst nil)
        {{nil nil}}
        {do
            (= {x} (fst lst))
            (= {xs} (unzip (tail lst)))
            (list
                (join (head x) (fst xs))
                (join (tail x) (snd xs)))}})
