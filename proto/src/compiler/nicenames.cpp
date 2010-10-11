#include "config.h"
#include <map>
#include "nicenames.h"
#include <stdio.h>

using namespace std;

string nicenames[] = 
  {"Alfa", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel",
   "India", "Juliet", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa",
   "Quebec", "Romeo", "Sierra", "Tango", "Uniform", "Victor", "Whiskey",
   "Xray", "Yankee", "Zulu", 
"Plural", "Beak", "Pronunciation", "Sow", "Sweeten", "Bribery", "Oar",
 "Electrician", "Homework", "Cultivator", "Motherly", "Redden", "Shilling",
 "Possessor", "Translator", "Scissors", "Headdress", "Calculator", "Handshake",
 "Sadden", "Motherhood", "Businesslike", "Donkey", "Deafen", "Widower",
 "Disrespect", "Rotten", "Weekday", "Scold", "Madden", "Saucer", "Obedient",
 "Cowardice", "Hindrance", "Conqueror", "Fatten", "Uppermost", "Lipstick",
 "Hurrah", "Punctual", "Moderation", "Brighten", "Parcel", "Deceit",
 "Notebook", "Canal", "Quart", "Cupboard", "Pastry", "Deepen", "Woolen",
 "Modesty", "Rubbish", "Companionship", "Cultivation", "Grammar", "Amongst",
 "Noun", "Baggage", "Bravery", "Hinder", "Pigeon", "Lighten", "Berry",
 "Whiten", "Barber", "Homecoming", "Liar", "Jealousy", "Jealous", "Ripen",
 "Procession", "Slippery", "Sour", "Chalk", "Handwriting", "Homemade",
 "Inclusive", "Tame", "Thorn", "Nuisance", "Feast", "Stocking", "Mend",
 "Bribe", "Tidy", "Whichever", "Multiplication", "Dissatisfy", "Overflow",
 "Paw", "Attentive", "Enclosure", "Discomfort", "Congratulate", "Cheat",
 "Rejoice", "Mat", "Coward", "Photography", "Breadth", "Bicycle",
 "Old-Fashioned", "Omission", "Weed", "Basin", "Spade", "Hesitation",
 "Descendant", "Correction", "Revenge", "Lengthen", "Deceive", "Strap",
 "Spoon", "Avoidance", "Thicken", "Reproduction", "Disappearance", "Ink",
 "Goat", "Everlasting", "Fry", "Ounce", "Congratulation", "Actress", "Flour",
 "Indoor", "Fright", "Complication", "Discontent", "Royalty", "Rivalry",
 "Cheese", "Loosen", "Flatten", "Heighten", "Rude", "Secrecy", "Worm", "Wheat",
 "Loaf", "Spill", "Niece", "Thirst", "Heavenly", "Greed", "Handkerchief",
 "Sharpen", "Outward", "Honesty", "Ornament", "Annoyance", "Dissatisfaction",
 "Chimney", "Inn", "Hammer", "Vowel", "Sock", "Obedience", "Persuasion",
 "Jewel", "Monkey", "Beast", "Grammatical", "Limb", "Lazy", "Ray", "Pearl",
 "Receipt", "Momentary", "Waist", "Disgust", "Adoption", "Confidential",
 "Framework", "Conquer", "Descent", "Pretense", "Hello", "Underneath",
 "Applaud", "Sometime", "Darken", "Cart", "Signature", "Scent", "Bucket",
 "Tailor", "Headache", "Conquest", "Gallon", "Merry", "Interruption",
 "Umbrella", "Cliff", "Inventor", "Sorrow", "Widen", "Rod", "Comb", "Elastic",
 "Castle", "Vain", "Messenger", "Sword", "Deaf", "Cough", "Meantime",
 "Librarian", "Ditch", "Sting", "Horizontal", "Spoil", "Coarse", "Offend",
 "Lately", "Pardon", "Cage", "Penny", "Tin", "Patriotic", "Nowadays",
 "Disregard", "Drawer", "Everyday", "Bean", "Deer", "Moonlight", "Competitor",
 "Sailor", "Wool", "Trunk", "Whoever", "Scorn", "Perfection", "Tighten",
 "Robbery", "Gaiety", "Aloud", "Imitate", "Fierce", "Pint", "Nursery", "Drown",
 "Cushion", "Nephew", "Knot", "Apology", "Ache", "Applause", "Shorten",
 "Hasten", "Purple", "Rake", "Excellence", "Inward", "Pig", "Stripe",
 "Selfish", "Upright", "Silk", "Stiffen", "Wicked", "Harden", "Enclose",
 "Hollow", "Rot", "Influential", "Landlord", "Copper", "Hook", "Imaginative",
 "Razor", "Diamond", "Pinch", "Sore", "Temper", "Ripe", "Cork", "Offense",
 "Apple", "Scenery", "Whistle", "Soup", "Repetition", "Shallow", "Heal", "Toy",
 "Annoy", "Lump", "Thumb", "Insult", "Tide", "Accustom", "Rabbit", "Wrist",
 "Miserable", "Collector", "Cultivate", "Theatrical", "Explosion",
 "Preference", "Deed", "Roast", "Rival", "Awkward", "Misery", "Rug", "Rubber",
 "Stove", "Saw", "Educator", "Carriage", "Ash", "Imaginary", "Suspicious",
 "Dip", "Daylight", "Defendant", "Ambitious", "Fur", "Essence", "Refresh",
 "Towel", "Upset", "Straw", "Pet", "Waiter", "Separation", "Rust", "Sew",
 "Damp", "Thief", "Customary", "Cave", "Ashamed", "Harvest", "Ribbon",
 "Elephant", "Cake", "Paste", "Simplicity", "Tobacco", "Voyage", "Prejudice",
 "Pan", "Confession", "Reproduce", "Delivery", "Collar", "Translation", "Axe",
 "Hut", "Obey", "Gap", "Ladder", "Suck", "Confident", "Wreck", "Basket",
 "Cautious", "Hatred", "Bake", "Shelf", "Juice", "Circular", "Alike", "Anyhow",
 "Hay", "Mouse", "Soften", "Cruel", "Tender", "Arrow", "Scrape", "Poverty",
 "Weaken", "Thunder", "Bush", "Remedy", "Invent", "Button", "Duck", "Wax",
 "Taxi", "Brass", "Explosive", "Tray", "Kneel", "Veil", "Shield", "Rid",
 "Airplane", "Envy", "Rope", "Boast", "Patience", "Postpone", "Resign", "Flag",
 "Fasten", "Explode", "Interfere", "Grease", "Solemn", "Laughter",
 "Calculation", "Curtain", "Skirt", "Steep", "Pad", "Complaint", "Lessen",
 "Needle", "Loyal", "Omit", "Classify", "Autumn", "Fond", "Coin", "Bleed",
 "Tribe", "Polite", "Decay", "Fancy", "Humble", "Crush", "Rescue", "Orange",
 "Weave", "Pen", "Interrupt", "Cream", "Rob", "Spit", "Flavor", "Steer",
 "Bell", "Bunch", "Elder", "Shower", "Curl", "Lamp", "Attraction",
 "Artificial", "Qualification", "Backward", "Businessman", "Rice", "Brick",
 "Float", "Stamp", "Splendid", "Sheep", "Invention", "Envelope", "Clever",
 "Mild", "Cape", "Descend", "Confess", "Fade", "Debt", "Lid", "Sauce",
 "Feather", "Shave", "Astonish", "Cottage", "Loyalty", "Shame", "Poison",
 "Polish", "Tune", "Ownership", "Noon", "Fork", "Destructive", "Disapprove",
 "Convenience", "Immense", "Treasure", "Trick", "Blade", "Disagree", "Mineral",
 "Caution", "Imitation", "Organ", "Outline", "Bowl", "Empire", "Pronounce",
 "Soap", "Jaw", "Regret", "Christmas", "Fever", "Idle", "Aunt", "Urgent",
 "Prevention", "Pump", "Permission", "Garage", "Kingdom", "Bow", "Multiply",
 "Swallow", "Lodge", "Bathe", "Tower", "Forbid", "Reward", "Toe", "Companion",
 "Bound", "Pin", "Steam", "Pity", "Vessel", "Decisive", "Warmth", "Inquire",
 "Dot", "Butter", "Rail", "Leather", "Bargain", "Shirt", "Bare", "Haste",
 "Thread", "Cap", "Fortune", "Tea", "Awake", "Despair", "Delicate", "Dismiss",
 "Pretend", "Ticket", "Reputation", "Ugly", "Wander", "Precious", "Holiday",
 "Lend", "Altogether", "Boil", "Clock", "Asleep", "Nowhere", "Boundary",
 "Crown", "Grateful", "Bundle", "Bath", "Friendship", "Swell", "Arch",
 "Plaster", "Spell", "Grind", "Nest", "Wrap", "Saddle", "Tent", "Silver",
 "Objection", "Scatter", "Mercy", "Behave", "Satisfaction", "Possession",
 "Eastern", "Tap", "Nail", "Salesman", "Educate", "Custom", "Politician",
 "Dine", "Modest", "Widow", "Quarrel", "Excessive", "Priest", "Weigh", "Curse",
 "Dig", "Complicate", "Moderate", "Horizon", "Convenient", "Seize", "Brave",
 "Stiff", "Gay", "Sweat", "Creep", "Slavery", "Lunch", "Scarce", "Straighten",
 "Barrel", "Telegraph", "Plow", "Spare", "Praise", "Spin", "Seldom",
 "Criminal", "Slope", "Mixture", "Ambition", "Funeral", "Owe", "Borrow",
 "Extraordinary", "Hesitate", "Journey", "Inquiry", "Noble", "Suspicion",
 "Translate", "Earnest", "Wealth", "Creature", "Faint", "Wipe", "Bury",
 "Sugar", "Swear", "Melt", "Split", "Nut", "Cheer", "Devil", "Meanwhile",
 "Bold", "Beard", "Pipe", "Beg", "Adventure", "Passenger", "Absent", "Neglect",
 "Resist", "Drum", "Broadcast", "Composition", "Scratch", "Stupid",
 "Admission", "Lonely", "Lung", "Bite", "Stroke", "Medicine", "Rub", "Leaf",
 "Cotton", "Sincere", "Pot", "Verse", "Insure", "Harm", "Prize", "Decrease",
 "Ceremony", "Ocean", "Insect", "Excuse", "Flood", "Greet", "Distant", "Dish",
 "Cheap", "Shine", "Anxious", "Dive", "Destruction", "Supper", "Tempt",
 "Shell", "Corn", "Bless", "Joke", "Flame", "Disappoint", "Border", "Pencil",
 "Beam", "Furniture", "Fault", "Fellowship", "Net", "Fate", "Dull", "Amuse",
 "Anywhere", "Breathe", "Reflection", "Punish", "Package", "Stir", "Belt",
 "Actor", "Courage", "Introduction", "Ruin", "Coal", "Merchant", "Mill",
 "Tongue", "Whip", "Tonight", "Sir", "Generous", "Proof", "Strengthen", "Tip",
 "Crash", "Bread", "Conscience", "Hunger", "Servant", "Allowance", "Desert",
 "Ton", "Storm", "Repair", "Peculiar", "Stomach", "Stuff", "Nurse", "Compete",
 "Self", "Temple", "Sake", "Roar", "Elect", "Complain", "Sacrifice", "Cat",
 "Overcome", "Treasury", "Weekend", "Neat", "Golden", "Glory", "Fortunate",
 "Sacred", "Tremble", "Bus", "Anxiety", "Whenever", "Blame", "Glad", "Mankind",
 "Raw", "Powder", "Puzzle", "Trap", "Cure", "Sympathetic", "Deserve", "Eager",
 "Cloth", "Certainty", "Persuade", "Wisdom", "Relieve", "Habit", "Attractive",
 "Sail", "Crop", "Kiss", "Recognition", "Extension", "Mud", "Fun", "Quantity",
 "Excess", "Hurt", "Anybody", "Whisper", "Valuable", "Royal", "Fold", "Pupil",
 "Interference", "Mad", "Elsewhere", "Qualify", "Plate", "Sand", "Tight",
 "Lesson", "Strict", "Reduction", "Insurance", "Funny", "Grand", "Cow",
 "Mechanism", "Camera", "Harbor", "String", "Discipline", "Arrest", "Worship",
 "Opposition", "Drag", "Calculate", "Grain", "Efficient", "Egg", "Connect",
 "Fan", "Anyway", "Everywhere", "Clerk", "Preach", "Verb", "Hire", "Till",
 "Pride", "Screw", "Resistance", "Burst", "Safety", "Kick", "Damage",
 "Confusion", "Pour", "Gentleman", "Delay", "Partner", "Stair", "Wing",
 "Steel", "Violence", "Pile", "Visitor", "Anger", "Republic", "Sorry", "Crime",
 "Prompt", "Wooden", "Holy", "Forgive", "Chicken", "Shut", "Expensive", "Gate",
 "Pink", "Numerous", "Attract", "Extra", "Queen", "Dare", "Virtue", "Wound",
 "Abroad", "Advice", "Photograph", "Salary", "Grave", "Solve", "Frighten",
 "Gold", "Confuse", "Lovely", "Row", "Loud", "Knock", "Necessity", "Accuse",
 "Shade", "Angry", "Frequency", "Restaurant", "Rank", "Sympathy", "Milk",
 "Heaven", "Ease", "Scientist", "Liquid", "Flesh", "Grace", "Possess",
 "Comparison", "Rent", "Extensive", "Defeat", "Disappear", "Efficiency", "Sad",
 "Sister", "Joy", "Childhood", "Fence", "Twist", "Flash", "Tube", "Discovery",
 "Noise", "Wise", "Liberty", "Sentence", "Highway", "Accident", "Universal",
 "Meal", "Spite", "Factory", "Absence", "Charm", "Threat", "Substance",
 "Tough", "Beneath", "Violent", "Bedroom", "Native", "Disturb", "Bag", "Ill",
 "Alive", "Iron", "Northern", "Sink", "March", "Entrance", "Chest", "Apart",
 "Breakfast", "Favorite", "Suggestion", "Grass", "Confidence", "Track",
 "Satisfactory", "Instant", "Afford", "Uncle", "Suspect", "Gift", "Joint",
 "Commerce", "Meat", "Wet", "Rush", "Crack", "Skin", "Male", "Ice", "Swim",
 "Calm", "Freeze", "Honest", "Bone", "Pocket", "Severe", "Wake", "Machinery",
 "Slide", "Chain", "Somewhere", "Shoe", "Onto", "Silence", "Afraid", "Breath",
 "Lake", "Card", "Dear", "Proud", "Surround", "Succeed", "Absolute",
 "Thorough", "Sick", "Yellow", "Star", "Plenty", "Tall", "Angle",
 "Conversation", "Shock", "Mechanic", "Strip", "Terrible", "Moon",
 "Competition", "Cup", "Bay", "Pleasant", "Particle", "Dozen", "Gentle",
 "Admire", "Agriculture", "Throat", "Completion", "Sweep", "Pale", "Salt",
 "Sensitive", "Yield", "Guilt", "Fruit", "Smoke", "Mix", "Shadow", "Somebody",
 "Bend", "Defend", "Musician", "Brain", "Stem", "Explore", "Relief", "Scale",
 "Cousin", "Slip", "Besides", "Colony", "Pack", "Earn", "Screen", "Silent",
 "Being", "Civilize", "Operator", "Shore", "Fish", "Ear", "Remind", "Tomorrow",
 "Risk", "Aside", "Band", "Mine", "Nose", "Loose", "Witness", "Pleasure",
 "Branch", "Roof", "Satisfy", "Hunt", "Gradual", "Metal", "Climb", "Lawyer",
 "Passage", "Politics", "Desk", "Intention", "Intend", "Fool", "Waste",
 "Customer", "Engine", "Quarter", "Victory", "Ought", "Proposal", "Wire",
 "Hide", "Female", "Furnish", "Smell", "Forth", "Coast", "Hurry", "Busy",
 "Universe", "Appearance", "Birth", "Adopt", "Pray", "Ancient", "Deliver",
 "Oppose", "Depth", "Threaten", "Steady", "Weather", "Smooth", "Rough", "Sky",
 "Unity", "Somehow", "Sheet", "Snow", "Formal", "Excellent", "Hat", "Disease",
 "Brown", "Avenue", "Stream", "Snake", "Jump", "Knee", "Welcome", "Mystery",
 "Advertise", "Protect", "Lean", "Stone", "Friendly", "Neighborhood", "Soul",
 "Entertain", "Key", "Curve", "Upper", "Arise", "Blind", "Urge", "Introduce",
 "Distinguish", "Soil", "Chairman", "Slave", "Cloud", "Prison", "Everybody",
 "Lord", "Profit", "Luck", "Guide", "Collect", "Beside", "Brush", "Coffee",
 "Minister", "Railroad", "Cook", "Presence", "Tool", "Valley", "Nobody",
 "Trust", "Delight", "Dirt", "Hate", "Neck", "Message", "Flat", "Tie",
 "Curious", "Literary", "Lift", "Sport", "Content", "Advise", "Baby", "Empty",
 "Wood", "Double", "Evil", "Safe", "Prefer", "Opposite", "Retire", "Mail",
 "Plain", "Correct", "Loan", "Fashion", "Seed", "Neighbor", "Guess", "Motion",
 "Wild", "Bird", "Dependence", "Agent", "Appoint", "Request", "Commercial",
 "Rare", "Exchange", "Worse", "Guard", "Shout", "Sweet", "Mistake", "Blow",
 "Village", "Knife", "Stretch", "Connection", "Stain", "Speech", "Lip",
 "Taste", "Gray", "Impossible", "Ordinary", "Belief", "Box", "Winter",
 "Forest", "Fresh", "Moreover", "Invite", "Gather", "Aim", "Solution", "Wheel",
 "Flower", "Scientific", "Yesterday", "Chair", "Bitter", "Ring", "Nice",
 "Tour", "Reserve", "Fat", "Library", "Thank", "Swing", "Fellow", "Daughter",
 "Clay", "Leadership", "Preserve", "Review", "Rain", "Collection", "Theater",
 "Narrow", "Variety", "Instrument", "Struggle", "Rich", "Belong", "Tire",
 "Tear", "Admit", "Bottle", "Dust", "Kitchen", "Flow", "Cattle", "Expense",
 "Battle", "Compose", "Perfect", "Beat", "Duty", "Hole", "Wine", "Everyone",
 "King", "Excite", "Weak", "Guest", "Seat", "Familiar", "Captain", "Cry",
 "Article", "Soldier", "Shelter", "Miss", "Warm", "Ideal", "Yard", "Signal",
 "Someone", "Dinner", "News", "Favor", "Garden", "Telephone", "Stick", "Crowd",
 "Escape", "Pure", "Representative", "Fast", "Weapon", "Page", "Solid",
 "Circle", "Reasonable", "Justice", "Soft", "Sea", "Ability", "Secret",
 "Newspaper", "Behavior", "Youth", "Mountain", "Bit", "Coat", "Thin",
 "Exercise", "Tend", "Title", "Perform", "Hardly", "Existence", "Refuse",
 "Wage", "Regular", "Match", "Advantage", "Reflect", "Importance", "Fame",
 "Pound", "Destroy", "Approve", "Wind", "Spread", "Sight", "Ahead", "Employee",
 "Lock", "Extent", "Reply", "Replace", "Broad", "Shop", "Propose", "Spot",
 "Finger", "Laugh", "Rapid", "Whatever", "Master", "Load", "Fix", "Discuss",
 "Weight", "Gas", "Straight", "Sharp", "Forward", "Clothe", "Council", "Trip",
 "Roll", "Repeat", "Pretty", "Sample", "Mass", "Bright", "Shoulder", "Affair",
 "Worry", "Governor", "Encourage", "Agency", "Motor", "Please", "Green",
 "Hill", "Mouth", "Citizen", "Block", "Speed", "Capital", "Wash", "Balance",
 "Director", "Attend", "Afternoon", "Daily", "Cross", "Lady", "Declare",
 "Bridge", "Marriage", "Practical", "Enemy", "Refer", "Choice", "Middle",
 "Tooth", "Difficulty", "Examine", "Discover", "Oil", "Camp", "Latter",
 "Comfort", "Thick", "Engineer", "Extreme", "Frame", "Shake", "Ball",
 "Dependent", "Fly", "Address", "Discussion", "Leg", "Staff", "Division",
 "Recommend", "Memory", "Distance", "Edge", "Extend", "Forget", "Model",
 "Murder", "Wave", "Faith", "Election", "Eat", "Worth", "Conscious", "Animal",
 "Radio", "Popular", "Post", "Skill", "Song", "Inch", "Poor", "Poem",
 "Prevent", "Glass", "Essential", "Audience", "Sun", "Burn", "Loss", "Rock",
 "Frequent", "Manufacture", "Suffer", "Hang", "Maybe", "Bar", "Freedom",
 "Base", "Film", "Arrive", "Literature", "Scene", "Shoot", "Blood", "Promise",
 "Pool", "Hospital", "Danger", "Corner", "Marry", "Quiet", "Brother", "Honor",
 "Bank", "Suit", "Travel", "Wrong", "Western", "Bill", "Opinion", "Finish",
 "Notice", "Strength", "Neither", "Progress", "Left", "Fit", "Manner",
 "Season", "Due", "Dream", "Electric", "Bed", "Hot", "Sleep", "Poet", "Check",
 "Dry", "Mention", "Sell", "Population", "Arrange", "Shape", "Advance",
 "Dollar", "Below", "Relate", "Depend", "Knowledge", "Pain", "Gain", "Anyone",
 "Dress", "Clean", "Boat", "Separate", "Strange", "Dog", "Gun", "Current",
 "Lay", "Degree", "Language", "Store", "Evening", "Chief", "Health", "Express",
 "Proper", "Enjoy", "Slight", "Hall", "Summer", "Combine", "Patient", "Army",
 "Cool", "Size", "Pick", "Listen", "Production", "Performance", "Save", "Heat",
 "Date", "Growth", "Settle", "Argue", "Prove", "Chance", "Pull", "Former",
 "Quality", "Relative", "Touch", "Police", "Race", "Hit", "Sing", "Difficult",
 "Temperature", "Official", "Surprise", "Imagine", "Indeed", "Especially",
 "Fair", "International", "Blue", "Husband", "Machine", "Occasion",
 "Influence", "Medical", "Quick", "Pattern", "Profession", "Relation",
 "Science", "Hair", "Lack", "Stock", "Lot", "Spring", "Association", "Attack",
 "Serious", "Permit", "Recognize", "Likely", "Opportunity", "Practice",
 "Trial", "Instead", "Strike", "Direction", "Beyond", "Window", "Foreign",
 "District", "Price", "Vote", "Square", "Park", "Sale", "Realize", "Operate",
 "Ride", "Suppose", "Win", "Wear", "Ship", "Attention", "Buy", "Club", "Cent",
 "Account", "Detail", "Game", "Improve", "River", "Smile", "Defense", "Employ",
 "East", "Basis", "Die", "Mere", "Accord", "Earth", "Bad", "Red", "Black",
 "Moral", "Committee", "Standard", "Origin", "Attempt", "Heavy", "Press",
 "Mark", "Basic", "Sudden", "Represent", "Cold", "Fill", "Drop", "Respect",
 "Product", "Unit", "Critic", "Happy", "English", "Character", "Alone", "Wide",
 "Deep", "Food", "Effective", "Station", "Suggest", "North", "Son", "Horse",
 "Ready", "Describe", "Raise", "Feed", "Contain", "Wish", "Immediate",
 "Private", "Doubt", "Officer", "Supply", "Visit", "Various", "Sign", "Spend",
 "Desire", "Share", "Decide", "Apply", "Space", "Paper", "Stay", "Experiment",
 "Story", "Drink", "Enter", "Island", "Slow", "Heart", "Secretary", "Allow",
 "Responsible", "Soon", "Mile", "Paint", "Judge", "Accept", "Wonder", "Demand",
 "Fear", "Trade", "Beauty", "Piece", "Outside", "Morning", "Property",
 "Pressure", "Further", "Operation", "Firm", "Claim", "Farm", "Wall", "Future",
 "Observe", "Peace", "Dance", "Regard", "Rule", "Surface", "Dark", "Modern",
 "Lie", "Front", "Hard", "Picture", "West", "Ground", "Charge", "Plant",
 "Offer", "Explain", "Prepare", "Market", "Entire", "Bear", "Table", "Father",
 "Stage", "Ago", "South", "Situation", "Watch", "Fight", "Test", "Purpose",
 "Remember", "Minute", "Success", "Century", "Rise", "University", "Total",
 "Term", "Send", "Produce", "Behind", "Color", "Letter", "Road", "Limit",
 "Treat", "Wife", "Short", "Draw", "Subject", "Common", "Cover", "Air",
 "Voice", "Learn", "Political", "Able", "Department", "Effort", "Money",
 "Lose", "Fire", "Half", "Organize", "Manage", "Society", "View", "Past",
 "Town", "Quite", "Across", "Mother", "Arm", "Agree", "Death", "Education",
 "Happen", "Material", "Land", "Party", "Support", "Tax", "Deal", "Court",
 "Necessary", "Friend", "Strong", "Above", "Class", "Step", "Cut", "Office",
 "Sound", "Recent", "Board", "Book", "Age", "Serve", "Cause", "Grow", "Music",
 "Perhaps", "Religion", "College", "Train", "Spirit", "Moment", "Receive",
 "Sure", "Level", "Determine", "Sit", "Record", "Although", "Easy", "Street",
 "Full", "Remain", "Rate", "Carry", "Hour", "Low", "Teach", "Walk", "Measure",
 "Today", "Talk", "Fall", "Field", "Condition", "Whole", "Idea", "Different",
 "Reach", "Understand", "Probable", "Body", "Inform", "Hope", "Door", "Best",
 "Concern", "Drive", "Month", "Student", "Active", "Speak", "Believe",
 "Doctor", "Appear", "Human", "Love", "Simple", "Care", "Foot", "Reason",
 "White", "Kind", "Often", "Per", "Rather", "Usual", "Matter", "Several",
 "God", "Girl", "Important", "Industry", "Law", "Car", "Direct", "Near", "Art",
 "Experience", "Mind", "Sense", "Act", "Family", "Expect", "Value", "Business",
 "Cost", "Boy", "Big", "Better", "Yet", "President", "Almost", "Young",
 "Nature", "Figure", "Room", "Social", "Church", "Week", "Company", "Night",
 "Far", "Woman", "Study", "Question", "Pay", "Power", "Certain", "Provide",
 "Side", "Center", "Bring", "Country", "Live", "Unite", "Light", "Hear",
 "Build", "War", "Upon", "Water", "Meet", "Force", "Case", "City", "Change",
 "Course", "Early", "Stand", "Play", "Fact", "Face", "Plan", "Eye", "Lead",
 "Consider", "Possible", "Govern", "Without", "Present", "Follow", "Person",
 "Interest", "Home", "Late", "Ask", "Against", "Want", "Leave", "Turn",
 "School", "General", "Thing", "Develop", "Mean", "Need", "House", "Show",
 "Write", "Tell", "Life", "Hand", "Nation", "Still", "World", "Little",
 "Place", "Feel", "Good", "Just", "People", "Well", "Great", "Must", "Way",
 "Day", "Most", "Think", "Give", "Such", "Work", "Use", "See", "Know", "Come",
 "Take", "Year", "State", "Man"
  };

int num_names = 2026;
int next_name = 0;

string get_next_nicename() {
  if(next_name<num_names) {
    return nicenames[next_name++];
  } else {
    char epoch[80]; sprintf(epoch,"%d",next_name/num_names);
    return nicenames[(next_name++)%num_names] + "-" + epoch;
  }
}

map<void*,string> namecache;
string nicename(void* value) {
  if(namecache[value]=="") namecache[value]=get_next_nicename();
  return namecache[value];
}